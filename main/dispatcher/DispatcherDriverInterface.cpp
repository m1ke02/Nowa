#include "Dispatcher.h"
#include "DispatcherUtils.h"
#include "esp_log.h"

#define TAG "DISP"

static void disp_connect(void *ctx, uint8_t idx, uint8_t bda[6]);
static void disp_disconnect(void *ctx, uint8_t idx);
static void disp_device_name(void *ctx, uint8_t idx, char *name);
static void disp_notification(void *ctx, uint8_t idx, ble_ancs_c_evt_notif_t *notif);
static void disp_attribute(void *ctx, uint8_t idx, uint32_t uid, ble_ancs_c_attr_t *attr);
static void disp_attributes_done(void *ctx, uint8_t idx, uint32_t uid);

static const std::vector<ble_ancs_c_notif_attr_id_val_t> basicAttrList {
    BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER,
    BLE_ANCS_NOTIF_ATTR_ID_DATE,
    BLE_ANCS_NOTIF_ATTR_ID_TITLE,
    BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE,
    BLE_ANCS_NOTIF_ATTR_ID_MESSAGE
};

/*static const std::vector<ble_ancs_c_notif_attr_id_val_t> auxAttrList {
    BLE_ANCS_NOTIF_ATTR_ID_TITLE,
    BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE,
    BLE_ANCS_NOTIF_ATTR_ID_MESSAGE
};*/

esp_err_t Dispatcher::initDriver(void) {
    ancs_handlers_t h;

    h.connect = disp_connect;
    h.disconnect = disp_disconnect;
    h.device_name = disp_device_name;
    h.notification = disp_notification;
    h.attribute = disp_attribute;
    h.attributes_done = disp_attributes_done;

    return ancs_init(this, &h); // ANCS driver is a singleton
}

static void disp_connect(void *ctx, uint8_t idx, uint8_t bda[6]) {
    Dispatcher *disp = static_cast<Dispatcher *>(ctx);
    disp->connectNP(idx, { bda[0], bda[1], bda[2], bda[3], bda[4], bda[5] });
    ESP_LOGI(TAG, "Connected as [%d]", idx);
    Notification *n = disp->getNPById(idx)->getLatestNotification();
    disp->m_prevLatestNotifications[idx] = n ? n->timeStamp : String();
}

static void disp_disconnect(void *ctx, uint8_t idx) {
    Dispatcher *disp = static_cast<Dispatcher *>(ctx);
    disp->disconnectNP(idx, false);
    ESP_LOGI(TAG, "Disconnected [%d]", idx);
}

static void disp_device_name(void *ctx, uint8_t idx, char *name) {
    Dispatcher *disp = static_cast<Dispatcher *>(ctx);
    disp->getNPById(idx)->setName(name);
    ESP_LOGI(TAG, "Device Name [%d]: %s", idx, name);
}

static void disp_notification(void *ctx, uint8_t idx, ble_ancs_c_evt_notif_t *notif) {
    Dispatcher *disp = static_cast<Dispatcher *>(ctx);
    if (notif->evt_id == BLE_ANCS_EVENT_ID_NOTIFICATION_ADDED || notif->evt_id == BLE_ANCS_EVENT_ID_NOTIFICATION_MODIFIED) {
        DispatcherUtils::printNotif(notif);

        bool empty = disp->m_attrRequestQueue[idx].empty();
        disp->m_attrRequestQueue[idx].push(std::make_pair(notif->notif_uid, basicAttrList));
        if (empty) {
            // Start read process if this is the first request
            AttrRequest r = disp->m_attrRequestQueue[idx].front();
            ESP_LOGI(TAG, "Starting immediately for UID %" PRIu32, r.first);
            ancs_send_attrs_request(idx, r.first, r.second.data(), r.second.size());
        }
    }
}

static void disp_attribute(void *ctx, uint8_t idx, uint32_t uid, ble_ancs_c_attr_t *attr) {
    Dispatcher *disp = static_cast<Dispatcher *>(ctx);
    DispatcherUtils::printNotifAttr(uid, attr);

    // Clean on first attribute
    AttrRequest r = disp->m_attrRequestQueue[idx].front();
    if (attr->attr_id == r.second[0]) {
        disp->m_notifBuffers[idx] = Notification();
    }

    switch (attr->attr_id) {
        case BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER: disp->m_notifBuffers[idx].appId = (char *)attr->p_attr_data; break;
        case BLE_ANCS_NOTIF_ATTR_ID_TITLE: disp->m_notifBuffers[idx].title = (char *)attr->p_attr_data; break;
        case BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE: disp->m_notifBuffers[idx].subTitle = (char *)attr->p_attr_data; break;
        case BLE_ANCS_NOTIF_ATTR_ID_MESSAGE: disp->m_notifBuffers[idx].message = (char *)attr->p_attr_data; break;
        case BLE_ANCS_NOTIF_ATTR_ID_DATE: disp->m_notifBuffers[idx].timeStamp = (char *)attr->p_attr_data; break;
        default: break;
    }
}

static void disp_attributes_done(void *ctx, uint8_t idx, uint32_t uid) {
    Dispatcher *disp = static_cast<Dispatcher *>(ctx);

    if (disp->m_attrRequestQueue[idx].empty()) {
        return; // Invalid state
    }

    disp->m_attrRequestQueue[idx].pop();

    // Notification filtering
    if (disp->m_notifBuffers[idx].appId.compare("com.apple.shortcuts") == 0) {
        // Add notification to the queue
        if (disp->m_notifBuffers[idx].timeStamp.compare(disp->m_prevLatestNotifications[idx]) > 0) {
            disp->getNPById(idx)->addNotification(disp->m_notifBuffers[idx]);
            ESP_LOGD(TAG, "Added!");
            // Request the other attributes
            //ESP_LOGI(TAG, "Requesting remaining attrs for UID %" PRIu32, uid);
            //disp->m_attrRequestQueue[idx].push(std::make_pair(uid, auxAttrList));
        } else {
            ESP_LOGD(TAG, "Oudated UID %" PRIu32, uid);
        }
    }

    if (!disp->m_attrRequestQueue[idx].empty()) {
        AttrRequest r = disp->m_attrRequestQueue[idx].front();
        ESP_LOGI(TAG, "Performing queued request for UID %" PRIu32, r.first);
        ancs_send_attrs_request(idx, r.first, r.second.data(), r.second.size());
    } else {
        ESP_LOGI(TAG, "Finished UID %" PRIu32, uid);
    }
}
