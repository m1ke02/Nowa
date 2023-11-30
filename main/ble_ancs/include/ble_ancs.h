#pragma once

#include <stdio.h>
#include "esp_system.h"
#include "ble_ancs_utils.h"

#define ANCS_PROFILE_NUM 4

typedef struct {
    void (*connect)(void *ctx, uint8_t idx, uint8_t bda[6]);
    void (*disconnect)(void *ctx, uint8_t idx);
    void (*device_name)(void *ctx, uint8_t idx, char *name);
    void (*notification)(void *ctx, uint8_t idx, ble_ancs_c_evt_notif_t *notif);
    void (*attribute)(void *ctx, uint8_t idx, uint32_t uid, ble_ancs_c_attr_t *attr);
    void (*attributes_done)(void *ctx, uint8_t idx, uint32_t uid);
} ancs_handlers_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ancs_init(void *ctx, ancs_handlers_t *h);
esp_err_t ancs_deinit(void *ctx);
bool ancs_is_initialized(void);
bool ancs_send_attrs_request(uint8_t idx, uint32_t uid, const ble_ancs_c_notif_attr_id_val_t attrs[], uint32_t attrs_length);
void ancs_dump_device_list(FILE *stream, const char *endl);

#ifdef __cplusplus
}
#endif
