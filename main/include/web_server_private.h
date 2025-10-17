#pragma once

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "esp_vfs.h"

#define MAX_OPEN_SOCKETS    7 // Must be in sync with HTTPD_DEFAULT_CONFIG()
#define FILE_PATH_MAX       (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE     (10240)

#ifndef MIN
#define MIN(a, b)       (((a) < (b)) ? (a) : (b))
#endif

typedef struct ws_user_context_tag {
    int open_fds[MAX_OPEN_SOCKETS];
    int open_fds_length;
    SemaphoreHandle_t opened_sem;
    QueueHandle_t mutex;
    esp_err_t (*receive_handler)(httpd_req_t *req, httpd_ws_frame_t *pkt);
} ws_user_context_t;

typedef struct server_context_tag {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} server_context_t;

extern httpd_handle_t server;
extern server_context_t context;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t web_ws_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif
