#include "dispatcher.h"

#include <stdint.h>
#include <string.h>
#include <vector>

#include "esp_log.h"

static void disp_connect(uint8_t idx, uint8_t bda[6]);
static void disp_disconnect(uint8_t idx);
static void disp_device_name(uint8_t idx, char *name);
static void disp_notification(uint8_t idx, ble_ancs_c_evt_notif_t *notif);
static void disp_attribute(uint8_t idx, uint32_t uid, ble_ancs_c_attr_t *attr);
static void disp_attributes_done(uint8_t idx, uint32_t uid);

using AttrRequest = std::pair<uint32_t, std::vector<ble_ancs_c_notif_attr_id_val_t>>;

static const std::vector<ble_ancs_c_notif_attr_id_val_t> basicAttrList = {
    BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER,
    BLE_ANCS_NOTIF_ATTR_ID_DATE
};

static const std::vector<ble_ancs_c_notif_attr_id_val_t> auxAttrList = {
    BLE_ANCS_NOTIF_ATTR_ID_TITLE,
    BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE,
    BLE_ANCS_NOTIF_ATTR_ID_MESSAGE
};


#define TAG "DISP"

/**@brief String literals for the iOS notification attribute types. Used when printing to UART. */
static char const * lit_attrid[BLE_ANCS_NB_OF_NOTIF_ATTR] =
{
    "App Identifier",
    "Title",
    "Subtitle",
    "Message",
    "Message Size",
    "Date",
    "Positive Action Label",
    "Negative Action Label"
};

/**@brief String literals for the iOS notification categories. used then printing to UART. */
static char const * lit_catid[BLE_ANCS_NB_OF_CATEGORY_ID] =
{
    "Other",
    "Incoming Call",
    "Missed Call",
    "Voice Mail",
    "Social",
    "Schedule",
    "Email",
    "News",
    "Health And Fitness",
    "Business And Finance",
    "Location",
    "Entertainment"
};

/**@brief String literals for the iOS notification event types. Used then printing to UART. */
static char const * lit_eventid[BLE_ANCS_NB_OF_EVT_ID] =
{
    "Added",
    "Modified",
    "Removed"
};

/**@brief String literals for the iOS notification attribute types. Used When printing to UART. */
static char const * lit_appid[BLE_ANCS_NB_OF_APP_ATTR] =
{
    "Display Name"
};

/**@brief Function for printing an iOS notification.
 *
 * @param[in] p_notif  Pointer to the iOS notification.
 */
static void notif_print(ble_ancs_c_evt_notif_t *p_notif)
{
    ESP_LOGI(TAG, "Notification");
    ESP_LOGI(TAG, "Event:       %s", lit_eventid[p_notif->evt_id]);
    ESP_LOGI(TAG, "Category ID: %s", lit_catid[p_notif->category_id]);
    ESP_LOGI(TAG, "Category Cnt:%" PRIu8, p_notif->category_count);
    ESP_LOGI(TAG, "UID:         %" PRIu32, p_notif->notif_uid);

    ESP_LOGI(TAG, "Flags:");
    if (p_notif->evt_flags.silent == 1)
    {
        ESP_LOGI(TAG, " Silent");
    }
    if (p_notif->evt_flags.important == 1)
    {
        ESP_LOGI(TAG, " Important");
    }
    if (p_notif->evt_flags.pre_existing == 1)
    {
        ESP_LOGI(TAG, " Pre-existing");
    }
    if (p_notif->evt_flags.positive_action == 1)
    {
        ESP_LOGI(TAG, " Positive Action");
    }
    if (p_notif->evt_flags.negative_action == 1)
    {
        ESP_LOGI(TAG, " Negative Action");
    }
}

static void bda_print(uint8_t bda[6]) {
    ESP_LOGI(TAG, "BDA %02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] p_attr Pointer to an iOS notification attribute.
 */
static void notif_attr_print(uint32_t uid, ble_ancs_c_attr_t *p_attr) {
    if (p_attr->attr_len != 0) {
        ESP_LOGI(TAG, "[%" PRIu32 "] NA %s: %s", uid, lit_attrid[p_attr->attr_id], p_attr->p_attr_data);
    } else if (p_attr->attr_len == 0) {
        ESP_LOGI(TAG, "NA %s: <No Data>", lit_attrid[p_attr->attr_id]);
    }
}

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] p_attr Pointer to an iOS App attribute.
 */
/*static void app_attr_print(ble_ancs_c_attr_t * p_attr)
{
    if (p_attr->attr_len != 0)
    {
        ESP_LOGI(TAG, "AA %s: %s", lit_appid[p_attr->attr_id], p_attr->p_attr_data);
    }
    else if (p_attr->attr_len == 0)
    {
        ESP_LOGI(TAG, "AA %s: <No Data>", lit_appid[p_attr->attr_id]);
    }
}*/

esp_err_t disp_init(void) {
    static ancs_handlers_t h;

    h.connect = disp_connect;
    h.disconnect = disp_disconnect;
    h.device_name = disp_device_name;
    h.notification = disp_notification;
    h.attribute = disp_attribute;
    h.attributes_done = disp_attributes_done;

    ESP_ERROR_CHECK(ancs_init(&h));

    return ESP_OK;
}

static void disp_connect(uint8_t idx, uint8_t bda[6]) {
    ESP_LOGI(TAG, "Connected as [%d]", idx);
    bda_print(bda);
}

static void disp_disconnect(uint8_t idx) {
    ESP_LOGI(TAG, "Disconnected [%d]", idx);
}

static void disp_device_name(uint8_t idx, char *name) {
    ESP_LOGI(TAG, "Device Name [%d]: %s", idx, name);
}

static std::queue<AttrRequest> attrRequestQueue[ANCS_PROFILE_NUM];
static bool attrRequestInProgress[ANCS_PROFILE_NUM] = {0};

static void disp_notification(uint8_t idx, ble_ancs_c_evt_notif_t *notif) {
    if (notif->evt_id == BLE_ANCS_EVENT_ID_NOTIFICATION_ADDED || notif->evt_id == BLE_ANCS_EVENT_ID_NOTIFICATION_MODIFIED) {
        notif_print(notif);

        if (!attrRequestInProgress[idx]) {
            // Start read process if this is the first request
            ESP_LOGI(TAG, "Starting immediately for UID %" PRIu32, notif->notif_uid);
            ancs_send_attrs_request(idx, notif->notif_uid, basicAttrList.data(), basicAttrList.size());
            attrRequestInProgress[idx] = true;
        } else {
            // Add to queue if consecutive
            ESP_LOGI(TAG, "Adding to queue for UID %" PRIu32, notif->notif_uid);
            attrRequestQueue[idx].push(std::make_pair(notif->notif_uid, basicAttrList));
        }
    }
}

static void disp_attribute(uint8_t idx, uint32_t uid, ble_ancs_c_attr_t *attr) {
    notif_attr_print(uid, attr);

    if (attr->attr_id == BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER &&
        strcmp((char *)(attr->p_attr_data), "com.apple.shortcuts") == 0) {
        ESP_LOGI(TAG, "Requesting remaining attrs for UID %" PRIu32, uid);
        attrRequestQueue[idx].push(std::make_pair(uid, auxAttrList));
    }
}

static void disp_attributes_done(uint8_t idx, uint32_t uid) {
    if (!attrRequestQueue[idx].empty()) {
        AttrRequest r = attrRequestQueue[idx].front();
        attrRequestQueue[idx].pop();
        ESP_LOGI(TAG, "Performing queued request for UID %" PRIu32, r.first);
        ancs_send_attrs_request(idx, r.first, r.second.data(), r.second.size());
    } else {
        ESP_LOGI(TAG, "Finished!");
        attrRequestInProgress[idx] = false;
    }
}
