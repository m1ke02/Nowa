#pragma once

#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"

extern MessageBufferHandle_t ancs_message_buffer;

esp_err_t ancs_init(void);
void ancs_dump_device_list(FILE *stream, const char *endl);
