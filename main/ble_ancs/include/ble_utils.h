#pragma once

#include "esp_system.h"
#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"

esp_gatt_status_t ble_utils_get_char (
    esp_gatt_if_t gattc_if,
    uint16_t conn_id,
    uint16_t service_start_handle,
    uint16_t service_end_handle,
    esp_bt_uuid_t char_uuid,
    esp_gatt_char_prop_t char_props,
    esp_gattc_char_elem_t *char_elem
);

esp_gatt_status_t ble_utils_get_descr (
    esp_gatt_if_t gattc_if,
    uint16_t conn_id,
    uint16_t service_start_handle,
    uint16_t service_end_handle,
    uint16_t char_for_ntf_handle,
    esp_gattc_descr_elem_t *descr_elem
);
