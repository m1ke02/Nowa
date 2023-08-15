#pragma once

#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"

extern MessageBufferHandle_t ancs_message_buffer;

typedef enum {
    ANCS_ATTR_TAG_APP_IDENTIFIER = 0,
    ANCS_ATTR_TAG_TITLE = 1,
    ANCS_ATTR_TAG_SUBTITLE = 2,
    ANCS_ATTR_TAG_MESSAGE = 3,
    ANCS_ATTR_TAG_MESSAGE_SIZE = 4,
    ANCS_ATTR_TAG_DATE = 5,
    ANCS_ATTR_TAG_POSITIVE_ACTION_LABEL = 6,
    ANCS_ATTR_TAG_NEGATIVE_ACTION_LABEL = 7,
    ANCS_ATTR_TAG_DEVICE_NAME = 0xFE,
    ANCS_ATTR_TAG_TERMINATOR = 0xFF,
} ancs_attr_tag_t;

esp_err_t ancs_init(void);
void ancs_dump_device_list(FILE *stream, const char *endl);
void ancs_dump_notification_list(FILE *stream, const char *endl);
