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
} State;

typedef struct {
	uint8_t alt_mouth;
	uint8_t dark_mode;
	uint8_t flipped;
	float rotation;
	float scale;
	__syscall_slong_t paw_hold_ns;
	int window_width, window_height;
} Config;

State state = {0};
Config config = {0};
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

void load_config(const char* config_path) {
	bool didnt_exist = access(config_path, F_OK) != 0;

	FILE* f = fopen(config_path, didnt_exist ? "w+" : "r+");
	if (!f) perror("fopen");

	// printf("didnt exist: %d\n", didnt_exist);
	if (didnt_exist)
		goto rewrite;

	bool ok = 1;
	ok &= fscanf(f, "dark_mode = %hhu\n", &config.dark_mode) == 1;
	ok &= fscanf(f, "alt_mouth = %hhu\n", &config.alt_mouth) == 1;
	ok &= fscanf(f, "flipped = %hhu\n", &config.flipped) == 1;
	ok &= fscanf(f, "rotation = %f\n", &config.rotation) == 1;
	ok &= fscanf(f, "scale = %f\n", &config.scale) == 1;
	ok &= fscanf(f, "paw_hold_ns = %ld\n", &config.paw_hold_ns) == 1;
	ok &= fscanf(f, "window_width = %d\n", &config.window_width) == 1;
	ok &= fscanf(f, "window_height = %d", &config.window_height) == 1;

	if (!ok)
		goto rewrite;

	goto cls;
rewrite:
	rewind(f);
	fprintf(f, "dark_mode = %hhu\n", config.dark_mode);
	fprintf(f, "alt_mouth = %hhu\n", config.alt_mouth);
	fprintf(f, "flipped = %hhu\n", config.flipped);
	fprintf(f, "rotation = %f\n", config.rotation);
	fprintf(f, "scale = %f\n", config.scale);
	fprintf(f, "paw_hold_ns = %ld\n", config.paw_hold_ns);
	fprintf(f, "window_width = %d\n", config.window_width);
	fprintf(f, "window_height = %d", config.window_height);
	fflush(f);
	ftruncate(fileno(f), ftell(f));
cls:
	fclose(f);
	glfwPostEmptyEvent();
}

void* config_thread(void* _) {
    char config_dir[PATH_MAX];
    const char* config_home = getenv("XDG_CONFIG_HOME");
    if (!config_home) {
		config_home = getenv("HOME");
		snprintf(config_dir, sizeof(config_dir), "%s/.config/bongocatl", config_home ? config_home : ".");
	} else {
		snprintf(config_dir, sizeof(config_dir), "%s/bongocatl", config_home ? config_home : ".test_config/");
	}

    mkdir_p(config_dir, 0755);

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

int main(void) {
	config.window_width = 550;
	config.window_height = 550;
	config.scale = 1.0f;
	config.paw_hold_ns = 50000000;

    // Init window
	SetConfigFlags(FLAG_WINDOW_TRANSPARENT);
    InitWindow(config.window_width, config.window_height, "bongocatl");
	EnableEventWaiting();

    // Load textures
    Texture2D textures[FILES_LEN];
    for (uint8_t i = 0; i < FILES_LEN; i++) {
		char path[PATH_MAX];

#ifdef NDEBUG
		snprintf(path, sizeof(path), "%s/%s", ASSETS_DIR, files[i]);
#else
		strcpy(path, files[i]);
#endif

        textures[i] = LoadTexture(path);

        if (textures[i].id == 0) {
            CloseWindow();
            return -1;
        }
    }

	int last_wwidth = config.window_width, last_wheight = config.window_height;
	RenderTexture2D screen = LoadRenderTexture(config.window_width, config.window_height);

	// UDS thread
	pthread_t client_tid, config_tid;
	pthread_create(&client_tid, NULL, client_thread, NULL);
	pthread_create(&config_tid, NULL, config_thread, NULL);

    while (!WindowShouldClose()) {
		if (config.window_width != last_wwidth || config.window_height != last_wheight) {
			UnloadRenderTexture(screen);
			screen = LoadRenderTexture(config.window_width, config.window_height);
			last_wwidth = config.window_width;
			last_wheight = config.window_height;
		}

		SetWindowSize(config.window_width, config.window_height);

        BeginDrawing();
        ClearBackground(CLITERAL(Color){0, 0, 0, 0});
		if (config.flipped || config.rotation != 0.0f) {
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

		if (config.flipped || config.rotation != 0.0f) {
			EndTextureMode();

			int sign = config.flipped ? -1 : 1;
			Rectangle src = { screen.texture.width, screen.texture.height, sign * screen.texture.width, -screen.texture.height }; 
			Rectangle dest = { ((float)config.window_width)/2.0f, ((float)config.window_height)/2.0f, screen.texture.width * config.scale, screen.texture.height * config.scale };
			Vector2 origin = { screen.texture.width/2.0f, screen.texture.height/2.0f }; 
			DrawTexturePro(screen.texture, src, dest, origin, config.rotation, WHITE);
		}

		char buf[64];
		snprintf(buf, sizeof(buf), "%zu", clicks);
		DrawText(buf, 0, 50, 20, RED);

		if (state.not_connected) {
			DrawText("Not connected", 100, 50, 40, RED);
		}

        EndDrawing();
    }

    // Unload textures
    for (uint8_t i = 0; i < FILES_LEN; i++) {
        UnloadTexture(textures[i]);
    }

    CloseWindow();
    return 0;
}
