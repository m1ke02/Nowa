#include "DispatcherUtils.h"

#include "esp_log.h"

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
void DispatcherUtils::printNotif(ble_ancs_c_evt_notif_t *p_notif)
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

void DispatcherUtils::printBDA(FILE *stream, BDA bda) {
    fprintf(stream, "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] p_attr Pointer to an iOS notification attribute.
 */
void DispatcherUtils::printNotifAttr(uint32_t uid, ble_ancs_c_attr_t *p_attr) {
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
/*void DispatcherUtils::printAppAttr(ble_ancs_c_attr_t * p_attr)
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
