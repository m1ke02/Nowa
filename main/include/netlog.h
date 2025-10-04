#pragma once

#include "esp_err.h"

#define NETLOG_MAX_MESSAGE_LENGTH 512
#define NETLOG_QUEUE_LENGTH 1024

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t netlog_init();
void netlog_task(void * pvParameters);
void netlog_queue_message(const char *message);

#ifdef __cplusplus
}
#endif
