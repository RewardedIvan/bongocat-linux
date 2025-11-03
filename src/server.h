#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void broadcast(const void* msg, size_t len);
void server_start();
void server_shutdown();

#ifdef __cplusplus
}
#endif
