#include "web_server_profile.h"
#include "web_server_private.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_http_server.h"

#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_app_desc.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "cJSON.h"

static const char *TAG = "webp";

static esp_err_t system_info_get_handler(httpd_req_t *req);
static esp_err_t mcu_restart_handler(httpd_req_t *req);
static esp_err_t firmware_update_post_handler(httpd_req_t *req);
static esp_err_t spiffs_update_post_handler(httpd_req_t *req);

static esp_err_t console_ws_receive_handler(httpd_req_t *req, httpd_ws_frame_t *pkt);

static bool mcu_restart_request = false;

static ws_user_context_t log_ws_ctx;
static ws_user_context_t console_ws_ctx;

esp_err_t web_profile_init()
{
    memset(&log_ws_ctx, 0, sizeof(log_ws_ctx));
    log_ws_ctx.opened_sem = xSemaphoreCreateBinary();
    if (log_ws_ctx.opened_sem == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateBinary failed");
        return ESP_ERR_NO_MEM;
    }
    log_ws_ctx.mutex = xSemaphoreCreateMutex();
    if (log_ws_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex failed");
        return ESP_ERR_NO_MEM;
    }

    memset(&console_ws_ctx, 0, sizeof(console_ws_ctx));
    console_ws_ctx.opened_sem = xSemaphoreCreateBinary();
    if (console_ws_ctx.opened_sem == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateBinary failed");
        return ESP_ERR_NO_MEM;
    }
    console_ws_ctx.mutex = xSemaphoreCreateMutex();
    if (console_ws_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex failed");
        return ESP_ERR_NO_MEM;
    }
    console_ws_ctx.receive_handler = console_ws_receive_handler;

    return ESP_OK;
}

esp_err_t web_profile_start()
{
    esp_err_t ret;

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"

    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/system_info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = &context
    };
    ret = httpd_register_uri_handler(server, &system_info_get_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_register_uri_handler failed with %d (%s)", ret, esp_err_to_name(ret));
        return ret;
    }

    /* URI handler for MCU restart */
    httpd_uri_t mcu_restart_uri = {
        .uri = "/api/mcu_restart",
        .method = HTTP_GET,
        .handler = mcu_restart_handler,
        .user_ctx = &context
    };
    ret = httpd_register_uri_handler(server, &mcu_restart_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_register_uri_handler failed with %d (%s)", ret, esp_err_to_name(ret));
        return ret;
    }

    /* URI handler for /firmware_update */
    httpd_uri_t firmware_update_uri = {
        .uri = "/api/firmware_update",
        .method = HTTP_POST,
        .handler = firmware_update_post_handler,
        .user_ctx = &context
    };
    ret = httpd_register_uri_handler(server, &firmware_update_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_register_uri_handler failed with %d (%s)", ret, esp_err_to_name(ret));
        return ret;
    }

    /* URI handler for /spiffs_update */
    httpd_uri_t spiffs_update_uri = {
        .uri = "/api/spiffs_update",
        .method = HTTP_POST,
        .handler = spiffs_update_post_handler,
        .user_ctx = &context
    };
    ret = httpd_register_uri_handler(server, &spiffs_update_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_register_uri_handler failed with %d (%s)", ret, esp_err_to_name(ret));
        return ret;
    }

    /* URI handler for /log websocket */
    httpd_uri_t log_ws_uri = {
        .uri        = "/log",
        .method     = HTTP_GET,
        .handler    = web_ws_handler,
        .user_ctx   = &log_ws_ctx,
        .is_websocket = true
    };
    log_ws_ctx.open_fds_length = 0;
    ret = httpd_register_uri_handler(server, &log_ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_register_uri_handler failed with %d (%s)", ret, esp_err_to_name(ret));
        return ret;
    }

    /* URI handler for /console websocket */
    httpd_uri_t console_ws_uri = {
        .uri        = "/console",
        .method     = HTTP_GET,
        .handler    = web_ws_handler,
        .user_ctx   = &console_ws_ctx,
        .is_websocket = true
    };
    console_ws_ctx.open_fds_length = 0;
    ret = httpd_register_uri_handler(server, &console_ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_register_uri_handler failed with %d (%s)", ret, esp_err_to_name(ret));
        return ret;
    }

    #pragma GCC diagnostic pop

    return ret;
}

esp_err_t web_profile_stop()
{
    return ESP_OK;
}

void web_profile_client_connect(int *fd)
{

}

void web_profile_client_disconnect(int *fd)
{
    if (mcu_restart_request) {
        esp_restart();
    }
}

/* Getting system info handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    char buf[24];

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddNumberToObject(root, "chip_model", chip_info.model);
    cJSON_AddNumberToObject(root, "chip_revision", chip_info.revision);
    snprintf(buf, sizeof(buf), "0x%02X", (uint8_t)chip_info.features);
    cJSON_AddStringToObject(root, "chip_features", buf);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON_AddStringToObject(root, "idf_version", app_desc->idf_ver);
    cJSON_AddStringToObject(root, "app_version", app_desc->version);
    cJSON_AddStringToObject(root, "build_date", app_desc->date);
    cJSON_AddStringToObject(root, "build_time", app_desc->time);

    esp_reset_reason_t rr = esp_reset_reason();
    cJSON_AddNumberToObject(root, "reset_reason", rr);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, "mac_wifi", buf);
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, "mac_bt", buf);

    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t mcu_restart_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Requesting MCU restart");
    mcu_restart_request = true;

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "MCU restart pending");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t firmware_update_post_handler(httpd_req_t *req)
{
    server_context_t *rest_context = (server_context_t *)req->user_ctx;
    esp_err_t ret = ESP_OK;
    int remaining = req->content_len;
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_prt = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *running_prt = esp_ota_get_running_partition();

    if (update_prt == NULL) {
        ESP_LOGE(TAG, "esp_ota_get_next_update_partition failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing partition: type %d/%d off 0x%08x size %d", update_prt->type, update_prt->subtype, update_prt->address, update_prt->size);
    ESP_LOGI(TAG, "Running partition: type %d/%d off 0x%08x size %d", running_prt->type, running_prt->subtype, running_prt->address, update_prt->size);

    ret = esp_ota_begin(update_prt, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed with %d (%s)", ret, esp_err_to_name(ret));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        // Read the data for the request
        if ((ret = httpd_req_recv(req, rest_context->scratch, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            ret = ESP_FAIL;
            break;
        }
        size_t bytes_read = ret;
        remaining -= bytes_read;
        ESP_LOGD(TAG, "Received %d bytes, remaining %d", bytes_read, remaining);
        ret = esp_ota_write(update_handle, rest_context->scratch, bytes_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed with %d (%s)", ret, esp_err_to_name(ret));
            break;
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Upload failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ret = esp_ota_end(update_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed with %d (%s)", ret, esp_err_to_name(ret));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ret = esp_ota_set_boot_partition(update_prt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed with %d (%s)", ret, esp_err_to_name(ret));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Updated successfully, requesting MCU restart");
    mcu_restart_request = true;

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "Updated successfully, MCU restart pending");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t spiffs_update_post_handler(httpd_req_t *req)
{
    server_context_t *rest_context = (server_context_t *)req->user_ctx;
    esp_err_t ret = ESP_OK;
    int remaining = req->content_len;
    const esp_partition_t *spiffs_prt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    uint32_t wrote_size = 0;

    if (spiffs_prt == NULL) {
        ESP_LOGE(TAG, "esp_partition_find_first failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "SPIFFS partition: type %d/%d off 0x%08x size %d", spiffs_prt->type, spiffs_prt->subtype, spiffs_prt->address, spiffs_prt->size);

    if (remaining != spiffs_prt->size) {
        ESP_LOGE(TAG, "Content length does not match partition size");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ret = esp_partition_erase_range(spiffs_prt, 0, spiffs_prt->size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_partition_erase_range failed with %d (%s)", ret, esp_err_to_name(ret));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        // Read the data for the request
        if ((ret = httpd_req_recv(req, rest_context->scratch, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            ret = ESP_FAIL;
            break;
        }
        size_t bytes_read = ret;
        remaining -= bytes_read;
        ESP_LOGD(TAG, "Received %d bytes, remaining %d", bytes_read, remaining);
        ret = esp_partition_write(spiffs_prt, wrote_size, rest_context->scratch, bytes_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_partition_write failed with %d (%s)", ret, esp_err_to_name(ret));
            break;
        }
        wrote_size += bytes_read;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Upload failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Updated successfully");

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Updated successfully");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t console_ws_receive_handler(httpd_req_t *req, httpd_ws_frame_t *pkt) {
    if (pkt->type == HTTPD_WS_TYPE_TEXT || pkt->type == HTTPD_WS_TYPE_BINARY) {
        if (pkt->len > 1 && pkt->payload[0] == '#') {
            ESP_LOGI(TAG, "Return error");
            return ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "MSG[%s]", pkt->payload);
            return httpd_ws_send_frame(req, pkt); // echo
        }
    }
    return ESP_OK;
}

void *web_get_log_user_ctx() {
    // Hide internals
    return &log_ws_ctx;
}

void *web_get_console_user_ctx() {
    // Hide internals
    return &console_ws_ctx;
}
