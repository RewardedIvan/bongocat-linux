#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static inline void get_socket_path(char* path) {
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");

    if (runtime_dir && strlen(runtime_dir) > 0) {
        snprintf(path, 108, "%s/bongocatl.sock", runtime_dir);
    } else {
        snprintf(path, 108, "/tmp/bongocatl.sock");
    }
}

typedef uint8_t Packet;
