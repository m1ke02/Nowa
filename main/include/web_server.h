#pragma once

#include "esp_err.h"
#include <stdbool.h>

#include "web_server_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t web_init();
esp_err_t web_start(const char *base_path);
esp_err_t web_stop();
bool web_is_started();

esp_err_t web_ws_send(void *ws_user_ctx, uint8_t *payload, int length);
int web_ws_get_num_clients(void *ws_user_ctx);
void web_ws_wait_for_client(void *ws_user_ctx, uint32_t block_time);

#ifdef __cplusplus
}
#endif
