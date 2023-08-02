#include "ble_utils.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include <string.h>

#define BLE_UTILS_TAG "BLE_UTILS"

esp_gatt_status_t ble_utils_read_char (
    esp_gatt_if_t gattc_if,
    uint16_t conn_id,
    uint16_t service_start_handle,
    uint16_t service_end_handle,
    esp_bt_uuid_t char_uuid,
    uint16_t *char_handle
) {
    uint16_t total_count = 0;
    esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(
        gattc_if,
        conn_id,
        ESP_GATT_DB_CHARACTERISTIC,
        service_start_handle,
        service_end_handle,
        0,
        &total_count
    );
    if (ret_status != ESP_GATT_OK) {
        ESP_LOGE(BLE_UTILS_TAG, "esp_ble_gattc_get_attr_count failed");
        return ret_status;
    }
    uint16_t char_count;
    for (uint16_t i = 0; i < total_count; i ++) {
        esp_gattc_char_elem_t char_elem_result = {0};
        char_count = 1;
        ret_status = esp_ble_gattc_get_all_char(
            gattc_if,
            conn_id,
            service_start_handle,
            service_end_handle,
            &char_elem_result,
            &char_count,
            i
        );
        if (ret_status != ESP_GATT_OK) {
            ESP_LOGE(BLE_UTILS_TAG, "esp_ble_gattc_get_all_char failed");
            return ret_status;
        }

        // ESP_LOGI(BLE_UTILS_TAG, "esp_ble_gattc_get_all_char: count=%u offset=%u found=%u h=%u u16=%u",
        //     total_count, i, char_count, char_elem_result.char_handle, char_elem_result.uuid.uuid.uuid16);

        if (memcmp(&char_elem_result.uuid, &char_uuid, sizeof(char_uuid)) == 0) {
            ESP_LOGI(BLE_UTILS_TAG, "Found characteristic at position %u UUID16=%04X", i, char_elem_result.uuid.uuid.uuid16);

            if (char_handle != NULL) {
                *char_handle = char_elem_result.char_handle;
            }

            ret_status = esp_ble_gattc_read_char(
                gattc_if,
                conn_id,
                char_elem_result.char_handle,
                ESP_GATT_AUTH_REQ_NONE
            );
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(BLE_UTILS_TAG, "esp_ble_gattc_read_char error, %d", __LINE__);
            }

            return ESP_GATT_OK;
        }
    }

    return ESP_GATT_NOT_FOUND;
}
