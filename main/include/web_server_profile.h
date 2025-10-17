#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t web_profile_init();
esp_err_t web_profile_start();
esp_err_t web_profile_stop();
void web_profile_client_connect(int *fd);
void web_profile_client_disconnect(int *fd);

void *web_get_log_user_ctx();
void *web_get_console_user_ctx();

#ifdef __cplusplus
}
#endif
