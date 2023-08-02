#pragma once

#include "esp_system.h"
#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"

esp_gatt_status_t ble_utils_read_char (
    esp_gatt_if_t gattc_if,
    uint16_t conn_id,
    uint16_t service_start_handle,
    uint16_t service_end_handle,
    esp_bt_uuid_t char_uuid,
    uint16_t *char_handle
);

