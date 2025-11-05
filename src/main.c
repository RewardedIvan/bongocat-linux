#include <raylib.h>
#include <GLFW/glfw3.h>

#include <linux/limits.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "shared.h"

const char *files[6] = {
	"assets/desk.png",
	"assets/bg.png",
	"assets/cat.png",
	"assets/mouth.png",
	"assets/paw-right.png",
	"assets/paw-left.png"
};

#define FILES_LEN sizeof(files) / sizeof(files[0])
#define ASSETS_DIR "/usr/local/share/bongocatl"

typedef struct {
	unsigned int left : 1;
	unsigned int right : 2;
	unsigned int alternate_state : 3;
	unsigned int not_connected : 4;
	unsigned int save_icon : 5;
} State;

typedef struct {
	uint8_t alt_mouth;
	uint8_t dark_mode;
	uint8_t flipped;
	float rotation;
	float scale;
	__syscall_slong_t paw_hold_ns;
	int window_width, window_height;
	uint8_t show_clicks;
	char font_path[PATH_MAX];
	float font_size, text_spacing;
	Vector2 clicks_pos;
	uint32_t clicks_color;
	float clicks_horizontal_alignment;
	uint8_t save_clicks;
	__syscall_slong_t save_interval_us;
	__syscall_slong_t save_icon_timeout_ns;
	int save_icon_pos_x;
	int save_icon_pos_y;
} Config;

#define CONFIG_FIELDS(X, X_STR) \
	X("dark_mode = %hhu", dark_mode) \
	X("alt_mouth = %hhu", alt_mouth) \
	X("flipped = %hhu", flipped) \
	X("rotation = %f", rotation) \
	X("scale = %f", scale) \
	X("paw_hold_ns = %ld", paw_hold_ns) \
	X("window_width = %d", window_width) \
	X("window_height = %d", window_height) \
	X_STR("font_path = %4095[^\n]", "font_path = %s", font_path) \
	X("font_size = %f", font_size) \
	X("text_spacing = %f", text_spacing) \
	X("show_clicks = %hhu", show_clicks) \
	X("clicks_pos_x = %f", clicks_pos.x) \
	X("clicks_pos_y = %f", clicks_pos.y) \
	X("clicks_color = 0x%X", clicks_color) \
	X("clicks_horizontal_alignment = %f", clicks_horizontal_alignment) \
	X("save_clicks = %hhu", save_clicks) \
	X("save_interval_us = %ld", save_interval_us) \
	X("save_icon_timeout_ns = %ld", save_icon_timeout_ns) \
	X("save_icon_pos_x = %d", save_icon_pos_x) \
	X("save_icon_pos_y = %d", save_icon_pos_y) \

State state = {0};
Config config = {0};
char config_dir[PATH_MAX];
size_t clicks = 0;

int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    if (!path || !*path) return -1;

    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp)-1] = '\0';
    len = strlen(tmp);

    // remove trailing slash
    if (tmp[len-1] == '/') tmp[len-1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0) {
                if (errno != EEXIST) return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0) {
        if (errno != EEXIST) return -1;
    }

    return 0;
}

void click_timer(union sigval arg) {
	state.right = 0;
	state.left = 0;
	glfwPostEmptyEvent();
}

void* client_thread(void* _) {
    int sock;
    struct sockaddr_un addr;
	state.not_connected = 1;
	glfwPostEmptyEvent();

	char socket_path[108] = {0};
	get_socket_path(socket_path);

    timer_t timer;
    struct sigevent sev = {0};
    struct itimerspec its = {0};

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = click_timer;

    timer_create(CLOCK_REALTIME, &sev, &timer);

	while (true) {
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sock < 0) { perror("socket"); return NULL; }

		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

		if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			perror("connect");
		} else {
			state.not_connected = 0;
			glfwPostEmptyEvent();
			while (true) {
				Packet iut;
				int n = read(sock, &iut, sizeof(iut));
				if (n != sizeof(iut)) break;

				state.right = state.alternate_state;
				state.left = !state.alternate_state;
				state.alternate_state =! state.alternate_state;
				clicks++;
				glfwPostEmptyEvent();

				its.it_value.tv_nsec = config.paw_hold_ns;
				timer_settime(timer, 0, &its, NULL);
			}
		}

		state.not_connected = 1;
		glfwPostEmptyEvent();
		close(sock);
		sleep(1);
	}
	return NULL;
}

void save_clicks();

void load_config(const char* config_path) {
	bool didnt_exist = access(config_path, F_OK) != 0;

	FILE* f = fopen(config_path, didnt_exist ? "w+" : "r+");
	if (!f) perror("fopen");

	// printf("didnt exist: %d\n", didnt_exist);
	if (didnt_exist)
		goto rewrite;

	char line[4096];

	bool ok = 1;

#define CFG_LOAD(fmt, var) ok &= fscanf(f, fmt "\n", &config.var) == 1;
#define CFG_LOAD_STR(fmt2, fmt, var) \
	ok &= fgets(line, sizeof(line), f) != NULL; \
	if (ok) { \
		if (sscanf(line, fmt2, config.var) != 1) \
			config.font_path[0] = '\0'; \
	}

	CONFIG_FIELDS(CFG_LOAD, CFG_LOAD_STR)

	if (!ok)
		goto rewrite;

	goto cls;
rewrite:
	rewind(f);
#define CFG_WRITE(fmt, var) fprintf(f, fmt "\n", (config.var));
#define CFG_WRITE_STR(fmt2, fmt, var) fprintf(f, fmt "\n", (config.var));
	CONFIG_FIELDS(CFG_WRITE, CFG_WRITE_STR)

	fflush(f);
	ftruncate(fileno(f), ftell(f));
cls:
	fclose(f);
	glfwPostEmptyEvent();
	save_clicks();
}

void* config_thread(void* _) {
    char config_file[PATH_MAX];
    snprintf(config_file, sizeof(config_file), "%s/cat.conf", config_dir);

    load_config(config_file);

    int fd = inotify_init();
    if (fd == -1) {
        perror("inotify_init");
        return NULL;
    }

    int wd = inotify_add_watch(fd, config_dir, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (wd == -1) {
        perror("inotify_add_watch");
        close(fd);
        return NULL;
    }

    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

    while (true) {
        ssize_t len = read(fd, buf, sizeof(buf));
        if (len == -1) {
            perror("read");
            break;
        }

        for (char *ptr = buf; ptr < buf + len; ) {
            struct inotify_event *event = (struct inotify_event *) ptr;

            if ((event->mask & (IN_MODIFY | IN_CREATE | IN_DELETE)) &&
                strcmp(event->name, "cat.conf") == 0) {
                // printf("Config file changed!\n");
                load_config(config_file);
            }

            ptr += sizeof(struct inotify_event) + event->len;
        }
    }

    close(fd);
    return NULL;
}

timer_t save_timer;

void save_icon_timer(union sigval arg) {
	state.save_icon = 0;
	glfwPostEmptyEvent();
}

char save_file[PATH_MAX];

void save_clicks() {
	// open
	FILE* f = fopen(save_file, "wb+");
	if (!f) perror("fopen");
	// write
	fwrite(&clicks, sizeof(clicks), 1, f);
	ftruncate(fileno(f), ftell(f));
	// close
	rewind(f);
	fclose(f);

	state.save_icon = 1;
	glfwPostEmptyEvent();

	struct itimerspec its = {0};
	its.it_value.tv_nsec = config.save_icon_timeout_ns;
	timer_settime(save_timer, 0, &its, NULL);
}

void* save_thread(void* _) {
	struct sigevent sev = {0};
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = save_icon_timer;

    timer_create(CLOCK_REALTIME, &sev, &save_timer);

	while (true) {
		usleep(config.save_interval_us);
		save_clicks();
	}
}

#ifdef NDEBUG
#define get_asset_path(path, asset) snprintf(path, sizeof(path), "%s/%s", ASSETS_DIR, asset);
#else
#define get_asset_path(path, asset) strcpy(path, asset);
#endif

int main(void) {
	config.window_width = 550;
	config.window_height = 550;
	config.scale = 1.0f;
	config.paw_hold_ns = 50000000;
	config.font_path[0] = '\0';
	config.font_size = 20.0f;
	config.text_spacing = 1.0f;
	config.clicks_horizontal_alignment = 1.0f;
	config.save_interval_us = 60e+6;
	config.save_icon_timeout_ns = 1e+8;

	{
		const char* config_home = getenv("XDG_CONFIG_HOME");
		const char* fmt = NULL;

		if (!config_home) {
			config_home = getenv("HOME");
			fmt = "%s/.config/bongocatl";
		} else {
			fmt = "%s/bongocatl";
		}

		snprintf(config_dir, sizeof(config_dir), fmt, config_home ? config_home : ".test_config/");
		mkdir_p(config_dir, 0755);

		snprintf(save_file, sizeof(save_file), "%s/clicks", config_dir);

		FILE* f = fopen(save_file, "rb+");
		if (!f) perror("fopen");
		else {
			fread(&clicks, sizeof(clicks), 1, f);
			fclose(f);
		}
	}

    // Init window
	SetConfigFlags(FLAG_WINDOW_TRANSPARENT);
    InitWindow(config.window_width, config.window_height, "bongocatl");
	EnableEventWaiting();

    // Load textures
    Texture2D textures[FILES_LEN];
    for (uint8_t i = 0; i < FILES_LEN; i++) {
		char path[PATH_MAX];

		get_asset_path(path, files[i]);
        textures[i] = LoadTexture(path);

        if (textures[i].id == 0) {
            CloseWindow();
            return -1;
        }
    }

    Texture2D save_icon_texture;
	{
		char path[PATH_MAX];
		get_asset_path(path, "assets/save.png");

		save_icon_texture = LoadTexture(path);

        if (save_icon_texture.id == 0) {
            CloseWindow();
            return -2;
        }
	}

	int last_wwidth = config.window_width,
		last_wheight = config.window_height;
	char current_font[PATH_MAX];
	current_font[0] = '\0';
	Font font = GetFontDefault();

	RenderTexture2D screen = LoadRenderTexture(config.window_width, config.window_height);

	// UDS thread
	pthread_t client_tid, config_tid, save_tid;
	pthread_create(&save_tid, NULL, save_thread, NULL);
	pthread_create(&client_tid, NULL, client_thread, NULL);
	pthread_create(&config_tid, NULL, config_thread, NULL);

    while (!WindowShouldClose()) {
		if (config.window_width != last_wwidth || config.window_height != last_wheight) {
			UnloadRenderTexture(screen);
			screen = LoadRenderTexture(config.window_width, config.window_height);
			last_wwidth = config.window_width;
			last_wheight = config.window_height;
		}

		if (strcmp(current_font, config.font_path) != 0) {
			// printf("current: '%s', config: '%s'\n", current_font, config.font_path);
			if (current_font[0] != '\0') {
				UnloadFont(font);
			}

			if (config.font_path[0] == '\0') {
				font = GetFontDefault();
			} else {
				font = LoadFont(config.font_path);
			}
			strncpy(current_font, config.font_path, sizeof(current_font));
		}

		SetWindowSize(config.window_width, config.window_height);

        BeginDrawing();
		bool useFb = config.flipped || config.rotation != 0.0f || config.scale != 1.0f;
		if (useFb) {
			BeginTextureMode(screen);
		}
        ClearBackground(CLITERAL(Color){0, 0, 0, 0});

        for (int i = 0; i < FILES_LEN; i++) {
			int xoff = 0;
			switch (i) {
				case 4: if (state.left) xoff = -800; break;
				case 5: if (state.right) xoff = -800; break;
				case 1: if (state.left) xoff = -800; break;
				case 3: if (config.alt_mouth) xoff = -800; break;
			}

			DrawTexture(textures[i], xoff -300, (config.dark_mode ? -450 : 0) + 75, WHITE);
        }

		if (useFb) {
			EndTextureMode();
			ClearBackground(CLITERAL(Color){0, 0, 0, 0});

			int sign = config.flipped ? -1 : 1;
			Rectangle src = { screen.texture.width, screen.texture.height, sign * screen.texture.width, -screen.texture.height }; 
			Rectangle dest = { ((float)config.window_width)/2.0f, ((float)config.window_height)/2.0f, screen.texture.width * config.scale, screen.texture.height * config.scale };
			Vector2 origin = { screen.texture.width/2.0f, screen.texture.height/2.0f }; 
			DrawTexturePro(screen.texture, src, dest, origin, config.rotation, WHITE);
		}

		if (config.show_clicks) {
			const char* str = TextFormat("%zu", clicks);
			Vector2 textSize = MeasureTextEx(font, str, config.font_size, config.text_spacing);
            Vector2 textPos = (Vector2) {
                config.clicks_pos.x + (-textSize.x * (config.clicks_horizontal_alignment * 0.5f)),
                config.clicks_pos.y + (-textSize.y * 0.5)
            };
			DrawTextEx(font, str, textPos, config.font_size, config.text_spacing, *((struct Color*)&config.clicks_color));
		}

		if (state.save_icon && config.save_icon_timeout_ns != 0) {
			DrawTexture(save_icon_texture, config.save_icon_pos_x, config.save_icon_pos_y, WHITE);
		}

		if (state.not_connected) {
			DrawText("Not connected", 100, 50, 40, RED);
		}

        EndDrawing();
    }

	save_clicks();

    // Unload textures
    for (uint8_t i = 0; i < FILES_LEN; i++) {
        UnloadTexture(textures[i]);
    }

	UnloadTexture(save_icon_texture);

	// pthread_join(save_tid, NULL);
	// pthread_join(client_tid, NULL);
	// pthread_join(config_tid, NULL);

    CloseWindow();
    return 0;
}
