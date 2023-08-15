#include "ancs.h"
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatt_common_api.h"
#include "ble_ancs_utils.h"
#include "ble_utils.h"
#include "esp_timer.h"

#define BLE_SVC_GAP_UUID16                                  0x1800
#define BLE_SVC_GAP_CHR_UUID16_DEVICE_NAME                  0x2a00
#define BLE_SVC_GAP_CHR_UUID16_APPEARANCE                   0x2a01
#define BLE_SVC_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS      0x2a04
#define BLE_SVC_GAP_CHR_UUID16_CENTRAL_ADDRESS_RESOLUTION   0x2aa6

#define TAG                                       "ANCS"
#define DEVICE_NAME                               "Nowa"
#define PROFILE_A_APP_ID                          0
#define PROFILE_B_APP_ID                          1
#define PROFILE_C_APP_ID                          2
#define PROFILE_D_APP_ID                          3
#define PROFILE_NUM                               4
#define ADV_CONFIG_FLAG                           (1 << 0)
#define SCAN_RSP_CONFIG_FLAG                      (1 << 1)
#define MESSAGE_BUFFER_STORAGE_SIZE               8192

#define MAX_NOTIF_ATTR_SIZE                       511

MessageBufferHandle_t ancs_message_buffer;

static uint8_t adv_config_done = 0;

static StaticMessageBuffer_t message_buffer_struct;

static uint8_t message_buffer_storage[MESSAGE_BUFFER_STORAGE_SIZE];

// ANCS UUID: 7905F431-B5CE-4E99-A40F-4B1E122D00D0
// In its basic form, the ANCS exposes three characteristics:
// Notification Source UUID: 9FBF120D-6301-42D9-8C58-25E699A21DBD (notifiable)
// Control Point UUID: 69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9 (writeable with response)
// Data Source UUID: 22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB (notifiable)
// Note: There may be more characteristics present in the ANCS than the three listed above.
// That said, an NC may ignore any characteristic it does not recognize.

static const esp_bt_uuid_t notification_source_char_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid.uuid128 = {0xbd, 0x1d, 0xa2, 0x99, 0xe6, 0x25, 0x58, 0x8c, 0xd9, 0x42, 0x01, 0x63, 0x0d, 0x12, 0xbf, 0x9f}
};

static const esp_bt_uuid_t control_point_char_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid.uuid128 = {0xd9, 0xd9, 0xaa, 0xfd, 0xbd, 0x9b, 0x21, 0x98, 0xa8, 0x49, 0xe1, 0x45, 0xf3, 0xd8, 0xd1, 0x69}
};

static const esp_bt_uuid_t data_source_char_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid.uuid128 = {0xfb, 0x7b, 0x7c, 0xce, 0x6a, 0xb3, 0x44, 0xbe, 0xb5, 0x4b, 0xd6, 0x24, 0xe9, 0xc6, 0xea, 0x22}
};

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static esp_bt_uuid_t anc_service_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid.uuid128 = {0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79}
};

static const esp_bt_uuid_t gap_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid.uuid16 = BLE_SVC_GAP_UUID16
};

static const esp_bt_uuid_t device_name_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid.uuid16 = BLE_SVC_GAP_CHR_UUID16_DEVICE_NAME
};

static const esp_bt_uuid_t appearance_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid.uuid16 = BLE_SVC_GAP_CHR_UUID16_APPEARANCE
};

static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

// config adv data
static esp_ble_adv_data_t adv_config = {
    .set_scan_rsp = false,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = ESP_BLE_APPEARANCE_GENERIC_HID,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// config scan response data
static esp_ble_adv_data_t scan_rsp_config = {
    .set_scan_rsp = true,
    .include_name = true,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x100,
    .adv_int_max        = 0x100,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_RPA_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t conn_id;
    struct {
        uint16_t service_start_handle;
        uint16_t service_end_handle;
        bool service_found;

        esp_gattc_char_elem_t notification_source_char_elem;
        esp_gattc_char_elem_t data_source_char_elem;
        esp_gattc_char_elem_t control_point_char_elem;
    } anc;
    struct {
        uint16_t service_start_handle;
        uint16_t service_end_handle;
        bool service_found;

        esp_gattc_char_elem_t device_name_elem;
        esp_gattc_char_elem_t appearance_elem;
    } gap;
    esp_bd_addr_t remote_bda;
    uint16_t MTU_size;

    ble_ancs_c_t ble_ancs_inst;

    union {
        struct { uint8_t id; char text[MAX_NOTIF_ATTR_SIZE]; };
        uint8_t data[MAX_NOTIF_ATTR_SIZE + 1];
    } attr_buffer;

    union {
        struct { uint8_t id; char text[63]; };
        uint8_t data[64]; // Must be <= MAX_NOTIF_ATTR_SIZE
    } device_name;

    uint16_t appearance;
};

static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM];

typedef enum {
   Unknown_command   = (0xA0), //The commandID was not recognized by the NP.
   Invalid_command   = (0xA1), //The command was improperly formatted.
   Invalid_parameter = (0xA2), // One of the parameters (for example, the NotificationUID) does not refer to an existing object on the NP.
   Action_failed     = (0xA3), //The action was not performed
} esp_error_code;

static char *Errcode_to_String(uint16_t status)
{
    char *Errstr = NULL;
    switch (status) {
        case Unknown_command:
            Errstr = "Unknown_command";
            break;
        case Invalid_command:
            Errstr = "Invalid_command";
            break;
        case Invalid_parameter:
            Errstr = "Invalid_parameter";
            break;
        case Action_failed:
            Errstr = "Action_failed";
            break;
        default:
            Errstr = "unknown_failed";
            break;
    }
    return Errstr;

}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGV(TAG, "GAP_EVT, event %d", event);

    switch (event) {
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "advertising start failed, error status = %x", param->adv_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "advertising start success");
        break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
        ESP_LOGI(TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
        /* Call the following function to input the passkey which is displayed on the remote device */
        //esp_ble_passkey_reply(heart_rate_profile_tab[HEART_PROFILE_APP_IDX].remote_bda, true, 0x00);
        break;
    case ESP_GAP_BLE_OOB_REQ_EVT: {
        ESP_LOGI(TAG, "ESP_GAP_BLE_OOB_REQ_EVT");
        uint8_t tk[16] = {1}; //If you paired with OOB, both devices need to use the same tk
        esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
        break;
    }
    case ESP_GAP_BLE_NC_REQ_EVT:
        /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
        show the passkey number to the user to confirm it with the number displayed by peer device. */
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        ESP_LOGI(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%" PRIu32, param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        /* send the positive(true) security response to the peer device to accept the security request.
        If not accept the security request, should send the security response with negative(false) accept value*/
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
        ///show the passkey number to the user to input it in the peer device.
        ESP_LOGI(TAG, "The passkey Notify number:%06" PRIu32, param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        esp_log_buffer_hex("addr", param->ble_security.auth_cmpl.bd_addr, ESP_BD_ADDR_LEN);
        ESP_LOGI(TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if (!param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    }
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
        if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "config local privacy failed, error status = %x", param->local_privacy_cmpl.status);
            break;
        }

        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_config);
        if (ret) {
            ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        } else {
            adv_config_done |= ADV_CONFIG_FLAG;
        }

        ret = esp_ble_gap_config_adv_data(&scan_rsp_config);
        if (ret) {
            ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        } else {
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
        }

        break;
    default:
        break;
    }
}

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    // Get profile by GATT if number
    int idx;
    for (idx = 0; idx < PROFILE_NUM; idx++) {
        if (gattc_if == gl_profile_tab[idx].gattc_if) {
            break;
        }
    }

    ESP_ERROR_CHECK(idx == PROFILE_NUM);

    ESP_LOGV(TAG, "GATT_EVT (PRF %u), event %d", idx, event);

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "REG_EVT: AppId=%u GattcIf=%u", param->reg.app_id, gattc_if);

        if (gattc_if == gl_profile_tab[PROFILE_A_APP_ID].gattc_if) { // Run only once
            esp_ble_gap_set_device_name(DEVICE_NAME);
            esp_ble_gap_config_local_icon (ESP_BLE_APPEARANCE_GENERIC_WATCH);
            //generate a resolvable random address
            esp_ble_gap_config_local_privacy(true);
        }
        break;
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "open failed, error status = %x", param->open.status);
            break;
        }
        ESP_LOGI(TAG, "ESP_GATTC_OPEN_EVT conn_id=%u", param->open.conn_id);
        gl_profile_tab[idx].conn_id = param->open.conn_id;
        esp_ble_set_encryption(param->open.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, param->open.conn_id);
        if (mtu_ret) {
            ESP_LOGE(TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK) {
            ESP_LOGE(TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        gl_profile_tab[idx].MTU_size = param->cfg_mtu.mtu;
        // memcpy(anc_service_uuid.uuid.uuid128, Apple_NC_UUID, 16);
        // esp_ble_gattc_search_service(gl_profile_tab[idx].gattc_if, gl_profile_tab[idx].conn_id, &apple_nc_uuid);
        esp_ble_gattc_search_service(gl_profile_tab[idx].gattc_if, gl_profile_tab[idx].conn_id, NULL);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        // ESP_LOGI(TAG, "ESP_GATTC_SEARCH_RES_EVT len=%u", param->search_res.srvc_id.uuid.len);
        // if (param->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16) {
        //     ESP_LOGI(TAG, "    UUID=%04X", param->search_res.srvc_id.uuid.uuid.uuid16);
        // } else if (param->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128) {
        //     ESP_LOGI(TAG, "    UUID=%08X-%04X-%04X-%04X-%04X%04X%04X",
        //         *((unsigned int *)&(param->search_res.srvc_id.uuid.uuid.uuid128[12])),
        //         *((uint16_t *)&(param->search_res.srvc_id.uuid.uuid.uuid128[10])),
        //         *((uint16_t *)&(param->search_res.srvc_id.uuid.uuid.uuid128[8])),
        //         *((uint16_t *)&(param->search_res.srvc_id.uuid.uuid.uuid128[6])),
        //         *((uint16_t *)&(param->search_res.srvc_id.uuid.uuid.uuid128[4])),
        //         *((uint16_t *)&(param->search_res.srvc_id.uuid.uuid.uuid128[2])),
        //         *((uint16_t *)&(param->search_res.srvc_id.uuid.uuid.uuid128[0]))
        //     );
        // }
        // ESP_LOGI(TAG, "    StartHandle=%04X EndHandle=%04X", param->search_res.start_handle, param->search_res.end_handle);
        if (memcmp (&param->search_res.srvc_id.uuid, &anc_service_uuid, sizeof(anc_service_uuid)) == 0) {
            gl_profile_tab[idx].anc.service_start_handle = param->search_res.start_handle;
            gl_profile_tab[idx].anc.service_end_handle = param->search_res.end_handle;
            gl_profile_tab[idx].anc.service_found = true;
            ESP_LOGI(TAG, "Found ANC service");
        } else if (memcmp (&param->search_res.srvc_id.uuid, &gap_service_uuid, sizeof(gap_service_uuid)) == 0) {
            gl_profile_tab[idx].gap.service_start_handle = param->search_res.start_handle;
            gl_profile_tab[idx].gap.service_end_handle = param->search_res.end_handle;
            gl_profile_tab[idx].gap.service_found = true;
            ESP_LOGI(TAG, "Found GAP service");
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");

        if (param->search_cmpl.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "search service failed, error status = %x", param->search_cmpl.status);
            break;
        }

        if (gl_profile_tab[idx].gap.service_found) {
            esp_gatt_status_t ret_status = ble_utils_get_char (
                gattc_if,
                gl_profile_tab[idx].conn_id,
                gl_profile_tab[idx].gap.service_start_handle,
                gl_profile_tab[idx].gap.service_end_handle,
                device_name_char_uuid,
                ESP_GATT_CHAR_PROP_BIT_READ,
                &gl_profile_tab[idx].gap.device_name_elem
            );
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "ble_utils_get_char_handle error, %d", __LINE__);
                goto gap_done;
            }
            ret_status = esp_ble_gattc_read_char(
                gattc_if,
                gl_profile_tab[idx].conn_id,
                gl_profile_tab[idx].gap.device_name_elem.char_handle,
                ESP_GATT_AUTH_REQ_NONE
            );
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "esp_ble_gattc_read_char error, %d", __LINE__);
                goto gap_done;
            }

            ret_status = ble_utils_get_char (gattc_if,
                                              gl_profile_tab[idx].conn_id,
                                              gl_profile_tab[idx].gap.service_start_handle,
                                              gl_profile_tab[idx].gap.service_end_handle,
                                              appearance_char_uuid,
                                              ESP_GATT_CHAR_PROP_BIT_READ,
                                              &gl_profile_tab[idx].gap.appearance_elem);
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "ble_utils_get_char_handle error, %d", __LINE__);
                goto gap_done;
            }
            ret_status = esp_ble_gattc_read_char(
                gattc_if,
                gl_profile_tab[idx].conn_id,
                gl_profile_tab[idx].gap.appearance_elem.char_handle,
                ESP_GATT_AUTH_REQ_NONE
            );
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "esp_ble_gattc_read_char error, %d", __LINE__);
                goto gap_done;
            }
            gap_done:
        }

        if (gl_profile_tab[idx].anc.service_found) {
            esp_gatt_status_t ret_status = ble_utils_get_char (
                gattc_if,
                gl_profile_tab[idx].conn_id,
                gl_profile_tab[idx].anc.service_start_handle,
                gl_profile_tab[idx].anc.service_end_handle,
                notification_source_char_uuid,
                ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                &gl_profile_tab[idx].anc.notification_source_char_elem
            );
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "ble_utils_get_char_handle error, %d", __LINE__);
                goto anc_done;
            }
            esp_ble_gattc_register_for_notify(
                gattc_if,
                gl_profile_tab[idx].remote_bda,
                gl_profile_tab[idx].anc.notification_source_char_elem.char_handle
            );
            ESP_LOGI(TAG, "Found Apple notification source char");

            ret_status = ble_utils_get_char (
                gattc_if,
                gl_profile_tab[idx].conn_id,
                gl_profile_tab[idx].anc.service_start_handle,
                gl_profile_tab[idx].anc.service_end_handle,
                data_source_char_uuid,
                ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                &gl_profile_tab[idx].anc.data_source_char_elem
            );
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "ble_utils_get_char_handle error, %d", __LINE__);
                goto anc_done;
            }
            esp_ble_gattc_register_for_notify(
                gattc_if,
                gl_profile_tab[idx].remote_bda,
                gl_profile_tab[idx].anc.data_source_char_elem.char_handle
            );
            ESP_LOGI(TAG, "Found Apple data source char");

            ret_status = ble_utils_get_char (
                gattc_if,
                gl_profile_tab[idx].conn_id,
                gl_profile_tab[idx].anc.service_start_handle,
                gl_profile_tab[idx].anc.service_end_handle,
                control_point_char_uuid,
                ESP_GATT_CHAR_PROP_BIT_WRITE,
                &gl_profile_tab[idx].anc.control_point_char_elem
            );
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "ble_utils_get_char_handle error, %d", __LINE__);
                goto anc_done;
            }
            ESP_LOGI(TAG, "Found Apple control point char");

            anc_done:
        }

        break;

    case ESP_GATTC_READ_CHAR_EVT:
        if (param->read.status != ESP_GATT_OK) {
            ESP_LOGI(TAG, "ESP_GATTC_READ_CHAR_EVT status %d", param->reg_for_notify.status);
            break;
        }

        if (param->read.handle == gl_profile_tab[idx].gap.device_name_elem.char_handle) {
            // Store null-terminated device name
            uint16_t len = (param->read.value_len > sizeof(gl_profile_tab[idx].device_name)-1) ?
                sizeof(gl_profile_tab[idx].device_name)-1 :
                param->read.value_len;
            memcpy(gl_profile_tab[idx].device_name.text, param->read.value, len);
            gl_profile_tab[idx].device_name.text[len] = '\0';
            ESP_LOGI(TAG, "Device name: '%s'", gl_profile_tab[idx].device_name.text);
        } else if (param->read.handle == gl_profile_tab[idx].gap.appearance_elem.char_handle) {
            // Store device appearance
            gl_profile_tab[idx].appearance = ((uint16_t)param->read.value[1] << 8) + param->read.value[0];
            ESP_LOGI(TAG, "Appearance: %04X", gl_profile_tab[idx].appearance);
        }
        // esp_log_buffer_hex("Data", param->read.value, param->read.value_len);
        break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (param->reg_for_notify.status != ESP_GATT_OK) {
            ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT status %d", param->reg_for_notify.status);
            break;
        }

        esp_gattc_descr_elem_t descr_elem;
        esp_gatt_status_t ret_status = ble_utils_get_descr (
            gattc_if,
            gl_profile_tab[idx].conn_id,
            gl_profile_tab[idx].anc.service_start_handle,
            gl_profile_tab[idx].anc.service_end_handle,
            param->reg_for_notify.handle,
            &descr_elem
        );
        if (ret_status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "ble_utils_get_descr error, %d", __LINE__);
            break;
        }
        uint8_t notify_en[2] = {0x01, 0x00};
        esp_ble_gattc_write_char_descr (
            gattc_if,
            gl_profile_tab[idx].conn_id,
            descr_elem.handle,
            sizeof(notify_en),
            (uint8_t *)&notify_en,
            ESP_GATT_WRITE_TYPE_RSP,
            ESP_GATT_AUTH_REQ_NONE
        );
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGV(TAG, "Rx notification: %u bytes", param->notify.value_len);
        esp_log_buffer_hex(TAG, param->notify.value, param->notify.value_len);
        if (param->notify.handle == gl_profile_tab[idx].anc.notification_source_char_elem.char_handle) {
            esp_err_t ret_status = ble_ancs_parse_notif(&gl_profile_tab[idx].ble_ancs_inst, param->notify.value, param->notify.value_len);
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "ble_ancs_parse_notif failed, error status = %x", ret_status);
            }
        } else if (param->notify.handle == gl_profile_tab[idx].anc.data_source_char_elem.char_handle) {
            ble_ancs_parse_get_attrs_response(&gl_profile_tab[idx].ble_ancs_inst, param->notify.value, param->notify.value_len);
        } else {
            ESP_LOGI(TAG, "unknown handle, receive notify value:");
        }
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (param->write.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "write descr failed, error status = %x", param->write.status);
            break;
        }
        ESP_LOGI(TAG, "Descriptor written successfully");
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(TAG, param->srvc_chg.remote_bda, 6);
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (param->write.status != ESP_GATT_OK) {
            char *Errstr = Errcode_to_String(param->write.status);
            if (Errstr) {
                 ESP_LOGE(TAG, "write control point error %s", Errstr);
            }
            break;
        }
        //ESP_LOGI(TAG, "Write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        // Every profile gets this notification
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT reason=0x%x", param->disconnect.reason);

        if (memcmp(gl_profile_tab[idx].remote_bda, param->disconnect.remote_bda, 6) != 0) {
            break;
        }

        ESP_LOGI(TAG, "Disconnecting this profile");
        gl_profile_tab[idx].anc.service_found = false;
        gl_profile_tab[idx].gap.service_found = false;
        memset(gl_profile_tab[idx].remote_bda, 0, sizeof(gl_profile_tab[idx].remote_bda));

        // must already be enabled
        // esp_ble_gap_start_advertising(&adv_params);

        break;
    case ESP_GATTC_CONNECT_EVT:
        // Every profile gets this notification
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT");

        if (idx == PROFILE_A_APP_ID) {
            esp_err_t ret = esp_ble_gap_start_advertising(&adv_params);
            ESP_LOGI(TAG, "Advertising restart: %x", ret);
        }

        bool handled = false;
        for (int new_idx = 0; new_idx < PROFILE_NUM; new_idx++) {
            if (memcmp(gl_profile_tab[new_idx].remote_bda, param->connect.remote_bda, 6) == 0) {
                handled = true;
                break;
            }
        }

        if (handled) {
            break; // This device is already being handled by new_idx profile
        }

        if (memcmp(gl_profile_tab[idx].remote_bda, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
            break; // This profile is already in use
        }

        ESP_LOGI(TAG, "Binding to this profile");
        esp_log_buffer_hex("bda", param->connect.remote_bda, 6);

        memcpy(gl_profile_tab[idx].remote_bda, param->connect.remote_bda, 6);

        // create gattc virtual connection
        esp_ble_gattc_open(gl_profile_tab[idx].gattc_if, gl_profile_tab[idx].remote_bda, BLE_ADDR_TYPE_RANDOM, true);
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_DIS_SRVC_CMPL_EVT");
        break;
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* Every profile has the same callback so call it directly */
    gattc_profile_event_handler(event, gattc_if, param);
}

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

/**@brief String literals for the iOS notification attribute types. Used When printing to UART. */
static char const * lit_appid[BLE_ANCS_NB_OF_APP_ATTR] =
{
    "Display Name"
};

/**@brief Function for printing an iOS notification.
 *
 * @param[in] p_notif  Pointer to the iOS notification.
 */
static void notif_print(ble_ancs_c_evt_notif_t * p_notif)
{
    ESP_LOGI(TAG, "Notification");
    ESP_LOGI(TAG, "Event:       %s", lit_eventid[p_notif->evt_id]);
    ESP_LOGI(TAG, "Category ID: %s", lit_catid[p_notif->category_id]);
    ESP_LOGI(TAG, "Category Cnt:%"PRIu8, p_notif->category_count);
    ESP_LOGI(TAG, "UID:         %"PRIu32, p_notif->notif_uid);

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

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] p_attr Pointer to an iOS notification attribute.
 */
static void notif_attr_print(ble_ancs_c_attr_t * p_attr)
{
    if (p_attr->attr_len != 0)
    {
        ESP_LOGI(TAG, "NA %s: %s", lit_attrid[p_attr->attr_id], p_attr->p_attr_data);
    }
    else if (p_attr->attr_len == 0)
    {
        ESP_LOGI(TAG, "NA %s: <No Data>", lit_attrid[p_attr->attr_id]);
    }
}

/**@brief Function for printing iOS notification attribute data.
 *
 * @param[in] p_attr Pointer to an iOS App attribute.
 */
static void app_attr_print(ble_ancs_c_attr_t * p_attr)
{
    if (p_attr->attr_len != 0)
    {
        ESP_LOGI(TAG, "AA %s: %s", lit_appid[p_attr->attr_id], p_attr->p_attr_data);
    }
    else if (p_attr->attr_len == 0)
    {
        ESP_LOGI(TAG, "AA %s: <No Data>", lit_appid[p_attr->attr_id]);
    }
}

static void send_to_stream(const uint8_t *data, size_t len) {
    size_t sent = xMessageBufferSend(ancs_message_buffer, data, len, 0);

    if (sent != len) {
        // TODO: Read and discard oldest element, then retry
        ESP_LOGE(TAG, "%s: Not enough free space", __func__);
        return;
    }

    ESP_LOGI(TAG, "Sent %u bytes", sent);
}

/**@brief Function for handling the Apple Notification Service client.
 *
 * @details This function is called for all events in the Apple Notification client that
 *          are passed to the application.
 *
 * @param[in] p_evt  Event received from the Apple Notification Service client.
 */
static void ancs_c_evt_handler(ble_ancs_c_evt_t * p_evt, void *ctx)
{
    static uint8_t attrs_request_buffer[BLE_ANCS_ATTR_DATA_MAX];
    uint32_t len;
    uint32_t idx = (uint32_t)ctx;

    ESP_LOGV(TAG, "ancs_c_evt_handler: %u uid=%"PRIu32, p_evt->evt_type, p_evt->notif.notif_uid);

    switch (p_evt->evt_type)
    {
        case BLE_ANCS_C_EVT_NOTIF:
            notif_print(&p_evt->notif);

            if (p_evt->notif.evt_id == BLE_ANCS_EVENT_ID_NOTIFICATION_ADDED || p_evt->notif.evt_id == BLE_ANCS_EVENT_ID_NOTIFICATION_MODIFIED) {
                gl_profile_tab[idx].ble_ancs_inst.parse_info.parse_state = BLE_ANCS_COMMAND_ID;

                // TODO: wait for next request if prev is pending

                len = ble_ancs_build_notif_attrs_request (&gl_profile_tab[idx].ble_ancs_inst, p_evt->notif.notif_uid, attrs_request_buffer, sizeof(attrs_request_buffer));
                if (len == 0) {
                    ESP_LOGE(TAG, "ble_ancs_build_notif_attrs_request: out of memory");
                    break;
                }
                esp_log_buffer_hex(TAG, attrs_request_buffer, len);
                esp_err_t ret_status = esp_ble_gattc_write_char(gl_profile_tab[idx].gattc_if,
                                                                gl_profile_tab[idx].conn_id,
                                                                gl_profile_tab[idx].anc.control_point_char_elem.char_handle,
                                                                len,
                                                                attrs_request_buffer,
                                                                ESP_GATT_WRITE_TYPE_RSP,
                                                                ESP_GATT_AUTH_REQ_NONE);
                if (ret_status != ESP_GATT_OK) {
                    ESP_LOGE(TAG, "esp_ble_gattc_write_char failed");
                    break;
                }
                // esp_get_notification_attributes(idx, &p_evt->notif.notif_uid, sizeof(p_attr)/sizeof(esp_noti_attr_list_t), p_attr);
            }
            break;

        case BLE_ANCS_C_EVT_NOTIF_ATTRIBUTE:
            notif_attr_print(&p_evt->attr);

            if (p_evt->attr.attr_len != 0) {
                // Add attribute ID & attribute text to stream buffer
                gl_profile_tab[idx].attr_buffer.id = (uint8_t)p_evt->attr.attr_id;
                send_to_stream(gl_profile_tab[idx].attr_buffer.data, 1 + strlen(gl_profile_tab[idx].attr_buffer.text));
            }

            // Is it the last attribute?
            if (ble_ancs_all_req_attrs_parsed(&gl_profile_tab[idx].ble_ancs_inst)) {
                gl_profile_tab[idx].device_name.id = ANCS_ATTR_TAG_DEVICE_NAME;
                send_to_stream(gl_profile_tab[idx].device_name.data, 1 + strlen(gl_profile_tab[idx].device_name.text));
                uint8_t term = ANCS_ATTR_TAG_TERMINATOR;
                send_to_stream(&term, 1);
            }

            break;
        default:
            // No implementation needed.
            break;
    }
}

static esp_err_t ancs_profile_init(int idx) {
    esp_err_t ret;

    gl_profile_tab[idx].gattc_cb = gattc_profile_event_handler,
    gl_profile_tab[idx].gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */

    // Init the ANCS client module
    memset(&gl_profile_tab[idx].ble_ancs_inst, 0, sizeof(gl_profile_tab[idx].ble_ancs_inst));
    gl_profile_tab[idx].ble_ancs_inst.evt_handler = ancs_c_evt_handler;
    gl_profile_tab[idx].ble_ancs_inst.ctx = (void *)idx;

    const ble_ancs_c_notif_attr_id_val_t attrs[] = {
        BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER,
        BLE_ANCS_NOTIF_ATTR_ID_TITLE,
        BLE_ANCS_NOTIF_ATTR_ID_MESSAGE,
        BLE_ANCS_NOTIF_ATTR_ID_DATE
    };

    for (uint32_t id = 0; id < sizeof(attrs) / sizeof(attrs[0]); id ++) {
        ret = ble_ancs_add_notif_attr(&gl_profile_tab[idx].ble_ancs_inst, attrs[id], (uint8_t *)gl_profile_tab[idx].attr_buffer.text, sizeof(gl_profile_tab[idx].attr_buffer.text));
        if (ret) {
            ESP_LOGE(TAG, "%s: ble_ancs_add_notif_attr(%u) failed, error code = %x", __func__, attrs[id], ret);
            return ret;
        }
    }
    // ret = ble_ancs_add_app_attr(&gl_profile_tab[idx].ble_ancs_inst, BLE_ANCS_APP_ATTR_ID_DISPLAY_NAME, gl_profile_tab[idx].attr_buffer, sizeof(gl_profile_tab[idx].attr_buffer));

    ret = esp_ble_gattc_app_register(idx);
    if (ret) {
        ESP_LOGE(TAG, "%s: esp_ble_gattc_app_register failed, error code = %x", __func__, ret);
        return ret;
    }

    return ESP_OK;
}

esp_err_t ancs_init(void)
{
    esp_err_t ret;

    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
    esp_log_level_set("nvs", ESP_LOG_VERBOSE);

    ancs_message_buffer = xMessageBufferCreateStatic(sizeof(message_buffer_storage), message_buffer_storage, &message_buffer_struct);
    if (ancs_message_buffer == NULL) {
        ESP_LOGE(TAG, "%s xMessageBufferCreateStatic failed", __func__);
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s: init controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s: enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "%s: init bluetooth", __func__);
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s: init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s: enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if (ret) {
        ESP_LOGE(TAG, "%s: gattc register error, error code = %x", __func__, ret);
        return ret;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "%s: gap register error, error code = %x", __func__, ret);
        return ret;
    }

    ret = ancs_profile_init(PROFILE_A_APP_ID);
    if (ret) {
        ESP_LOGE(TAG, "%s: ancs_profile_init failed, error code = %x", __func__, ret);
        return ret;
    }
    ret = ancs_profile_init(PROFILE_B_APP_ID);
    if (ret) {
        ESP_LOGE(TAG, "%s: ancs_profile_init failed, error code = %x", __func__, ret);
        return ret;
    }
    ret = ancs_profile_init(PROFILE_C_APP_ID);
    if (ret) {
        ESP_LOGE(TAG, "%s: ancs_profile_init failed, error code = %x", __func__, ret);
        return ret;
    }
    ret = ancs_profile_init(PROFILE_D_APP_ID);
    if (ret) {
        ESP_LOGE(TAG, "%s: ancs_profile_init failed, error code = %x", __func__, ret);
        return ret;
    }

    ret = esp_ble_gatt_set_local_mtu(131/*500*/);
    if (ret) {
        ESP_LOGE(TAG, "%s: set local MTU failed, error code = %x", __func__, ret);
    }

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    //set static passkey
    uint32_t passkey = 123456;
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
    /* If your BLE device acts as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the master;
    If your BLE device acts as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    return ESP_OK;
}

void ancs_dump_device_list(FILE *stream, const char *endl) {
    bool empty = true;
    for (int idx = 0; idx < PROFILE_NUM; idx ++) {
        if (memcmp(gl_profile_tab[idx].remote_bda, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
            empty = false;
            fprintf(stream, "[%i] %04x %s @ %02x:%02x:%02x:%02x:%02x:%02x%s",
                idx+1,
                gl_profile_tab[idx].appearance,
                gl_profile_tab[idx].device_name.text,
                gl_profile_tab[idx].remote_bda[0],
                gl_profile_tab[idx].remote_bda[1],
                gl_profile_tab[idx].remote_bda[2],
                gl_profile_tab[idx].remote_bda[3],
                gl_profile_tab[idx].remote_bda[4],
                gl_profile_tab[idx].remote_bda[5],
                endl
            );
        }
    }
    if (empty) {
        fprintf(stream, "<No devices>%s", endl);
    }
}

void ancs_dump_notification_list(FILE *stream, const char *endl) {
    union {
        struct { uint8_t id; char text[MAX_NOTIF_ATTR_SIZE]; };
        uint8_t data[MAX_NOTIF_ATTR_SIZE + 1];
    } attr_buffer;

    uint32_t ctr = 0;

    while (true) {
        size_t len = xMessageBufferReceive(ancs_message_buffer, attr_buffer.data, sizeof(attr_buffer), 0);
        if (len == 0) {
            // No more items
            if (ctr == 0) {
                fprintf(stream, "<No messages>%s", endl);
            }
            return;
        }

        if ((len > 1) && (len < MAX_NOTIF_ATTR_SIZE)) {
            attr_buffer.text[len] = '\0';
        } else if (len == MAX_NOTIF_ATTR_SIZE) {
            attr_buffer.text[MAX_NOTIF_ATTR_SIZE - 1] = '\0'; // TODO: check max size
        }

        switch (attr_buffer.id) {
            case ANCS_ATTR_TAG_DEVICE_NAME:
                fprintf(stream, "Dev %s%s", attr_buffer.text, endl);
                break;
            case ANCS_ATTR_TAG_APP_IDENTIFIER:
                fprintf(stream, "App %s%s", attr_buffer.text, endl);
                break;
            case ANCS_ATTR_TAG_MESSAGE:
                fprintf(stream, "Msg %s%s", attr_buffer.text, endl);
                break;
            case ANCS_ATTR_TAG_TERMINATOR:
                fprintf(stream, "--- End of message %"PRIu32" ---%s", ctr + 1, endl);
                ctr ++;
                break;
        }
    }
}
