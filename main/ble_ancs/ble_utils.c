#include "ble_utils.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include <string.h>

#define TAG "BLEU"

esp_gatt_status_t ble_utils_get_char (
    esp_gatt_if_t gattc_if,
    uint16_t conn_id,
    uint16_t service_start_handle,
    uint16_t service_end_handle,
    esp_bt_uuid_t char_uuid,
    esp_gatt_char_prop_t char_props,
    esp_gattc_char_elem_t *char_elem
) {
    if (char_elem == NULL) {
        return ESP_GATT_INVALID_HANDLE;
    }
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
        ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count failed");
        return ret_status;
    }
    if (total_count == 0) {
        return ESP_GATT_NOT_FOUND;
    }
    uint16_t char_count;
    for (uint16_t i = 0; i < total_count; i ++) {
        memset(&(char_elem->uuid), 0, sizeof(char_elem->uuid));
        char_count = 1;
        ret_status = esp_ble_gattc_get_all_char(
            gattc_if,
            conn_id,
            service_start_handle,
            service_end_handle,
            char_elem,
            &char_count,
            i
        );
        if (ret_status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "esp_ble_gattc_get_all_char failed");
            return ret_status;
        }

        // ESP_LOGI(TAG, "esp_ble_gattc_get_all_char: count=%u offset=%u found=%u h=%u u16=%u",
        //     total_count, i, char_count, char_elem_result.char_handle, char_elem_result.uuid.uuid.uuid16);

        if ((memcmp(&(char_elem->uuid), &char_uuid, sizeof(char_elem->uuid)) == 0) &&
            ((char_elem->properties & char_props) != 0)) {
            ESP_LOGD(TAG, "Found characteristic at position %u UUID16=%04X", i, char_elem->uuid.uuid.uuid16);

            return ESP_GATT_OK;
        }
    }

    return ESP_GATT_NOT_FOUND;
}

esp_gatt_status_t ble_utils_get_descr (
    esp_gatt_if_t gattc_if,
    uint16_t conn_id,
    uint16_t service_start_handle,
    uint16_t service_end_handle,
    uint16_t char_for_ntf_handle,
    esp_gattc_descr_elem_t *descr_elem
) {
    if (descr_elem == NULL) {
        return ESP_GATT_INVALID_HANDLE;
    }
    uint16_t total_count = 0;
    esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(
        gattc_if,
        conn_id,
        ESP_GATT_DB_DESCRIPTOR,
        service_start_handle,
        service_end_handle,
        char_for_ntf_handle,
        &total_count
    );
    if (ret_status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count failed");
        return ret_status;
    }
    if (total_count == 0) {
        return ESP_GATT_NOT_FOUND;
    }
    uint16_t descr_count;
    for (uint16_t i = 0; i < total_count; i ++) {
        memset(&(descr_elem->uuid), 0, sizeof(descr_elem->uuid));
        descr_count = 1;
        ret_status = esp_ble_gattc_get_all_descr(
            gattc_if,
            conn_id,
            char_for_ntf_handle,
            descr_elem,
            &descr_count,
            i
        );
        if (ret_status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "esp_ble_gattc_get_all_descr failed");
            return ret_status;
        }

        if ((descr_elem->uuid.len == ESP_UUID_LEN_16) &&
            (descr_elem->uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)) {
            ESP_LOGD(TAG, "Found CCCD at position %u", i);

            return ESP_GATT_OK;
        }
    }

    return ESP_GATT_NOT_FOUND;
}
