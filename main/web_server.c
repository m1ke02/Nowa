#include "web_server.h"
#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_app_desc.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"

static const char *TAG = "web";

#define FILE_PATH_MAX       (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE     (10240)
#define MAX_OPEN_SOCKETS    7 // Must be in sync with HTTPD_DEFAULT_CONFIG()

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

typedef struct server_context_tag {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} server_context_t;

typedef struct ws_user_context_tag {
    int open_fds[MAX_OPEN_SOCKETS];
    int open_fds_length;
    SemaphoreHandle_t opened_sem;
    QueueHandle_t mutex;
    esp_err_t (*receive_handler)(httpd_req_t *req, httpd_ws_frame_t *pkt);
} ws_user_context_t;

typedef struct ws_sess_context_tag {
    ws_user_context_t *user_ctx;
    int fd;
} ws_sess_context_t;

static httpd_handle_t server = NULL;
static httpd_config_t config = HTTPD_DEFAULT_CONFIG();
static server_context_t context;
static ws_user_context_t log_ws_ctx;
static ws_user_context_t console_ws_ctx;

static bool mcu_restart_request = false;

static esp_err_t system_info_get_handler(httpd_req_t *req);
static esp_err_t mcu_restart_handler(httpd_req_t *req);
static esp_err_t rest_common_get_handler(httpd_req_t *req);
static esp_err_t console_ws_receive_handler(httpd_req_t *req, httpd_ws_frame_t *pkt);

static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath);
static void ws_free_ctx_handler(void *ctx);
static esp_err_t ws_handler(httpd_req_t *req);

static void client_disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void client_connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void request_complete_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

esp_err_t web_init() {
    config.uri_match_fn = httpd_uri_match_wildcard;

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

esp_err_t web_start(const char *base_path)
{
    if (server != NULL) {
        ESP_LOGE(TAG, "Server already started");
        return ESP_FAIL;
    }

    strlcpy(context.base_path, base_path, sizeof(context.base_path));

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start server failed");
        return ret;
    }

    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/system_info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = &context
    };
    ret = httpd_register_uri_handler(server, &system_info_get_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cannot register URI handler");
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
        ESP_LOGE(TAG, "Cannot register URI handler");
        return ret;
    }

    /* URI handler for /log websocket */
    httpd_uri_t log_ws_uri = {
        .uri        = "/log",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = &log_ws_ctx,
        .is_websocket = true
    };
    log_ws_ctx.open_fds_length = 0;
    ret = httpd_register_uri_handler(server, &log_ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cannot register URI handler");
        return ret;
    }

    /* URI handler for /console websocket */
    httpd_uri_t console_ws_uri = {
        .uri        = "/console",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = &console_ws_ctx,
        .is_websocket = true
    };
    console_ws_ctx.open_fds_length = 0;
    ret = httpd_register_uri_handler(server, &console_ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cannot register URI handler");
        return ret;
    }

    /* URI handler for getting web server files (must be the last one!) */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = &context
    };
    ret = httpd_register_uri_handler(server, &common_get_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cannot register URI handler");
        return ret;
    }

    esp_event_handler_register(ESP_HTTP_SERVER_EVENT, HTTP_SERVER_EVENT_ON_CONNECTED, client_connect_handler, NULL);
    esp_event_handler_register(ESP_HTTP_SERVER_EVENT, HTTP_SERVER_EVENT_DISCONNECTED, client_disconnect_handler, NULL);
    esp_event_handler_register(ESP_HTTP_SERVER_EVENT, HTTP_SERVER_EVENT_SENT_DATA, request_complete_handler, NULL);

    return ESP_OK;
}

esp_err_t web_stop()
{
    if (server == NULL) {
        ESP_LOGE(TAG, "Server already stopped");
        return ESP_FAIL;
    }

    // Stop the httpd server
    esp_err_t ret = httpd_stop(server);
    server = NULL;

    esp_event_handler_unregister(ESP_HTTP_SERVER_EVENT, HTTP_SERVER_EVENT_ON_CONNECTED, client_connect_handler);
    esp_event_handler_unregister(ESP_HTTP_SERVER_EVENT, HTTP_SERVER_EVENT_DISCONNECTED, client_disconnect_handler);

    return ret;
}

/*
 * Broadcast message to all clients connected to this websocket
 */
esp_err_t web_ws_send(void *ws_user_ctx, uint8_t *payload, int length)
{
    ws_user_context_t *ctx = (ws_user_context_t *)ws_user_ctx;

    if (server == NULL) {
        ESP_LOGE(TAG, "Server stopped");
        return ESP_FAIL;
    }

    httpd_ws_frame_t frame = {.type = HTTPD_WS_TYPE_TEXT, .payload = payload, .len = length};
    for (int i = 0; i < ctx->open_fds_length; i++) {
        esp_err_t ret = httpd_ws_send_data(server, ctx->open_fds[i], &frame);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_send_data failed with %d", ret);
            return ret;
        }
    }

    return ESP_OK;
}

int web_ws_get_num_clients(void *ws_user_ctx) {
    ws_user_context_t *ctx = (ws_user_context_t *)ws_user_ctx;
    return ctx->open_fds_length;
}

void web_ws_wait_for_client(void *ws_user_ctx, uint32_t block_time) {
    ws_user_context_t *ctx = (ws_user_context_t *)ws_user_ctx;
    xSemaphoreTake(ctx->opened_sem, (TickType_t)block_time);
}

void *web_get_log_user_ctx() {
    // Hide internals
    return &log_ws_ctx;
}

void *web_get_console_user_ctx() {
    // Hide internals
    return &console_ws_ctx;
}

static void client_disconnect_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    int *fd = (int *)event_data;
    ESP_LOGI(TAG, "client_disconnect: FD=%d", *fd);
}

static void client_connect_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    int *fd = (int *)event_data;
    ESP_LOGI(TAG, "client_connect: FD=%d", *fd);
}

static void request_complete_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    //esp_http_server_event_data *data = (esp_http_server_event_data *)event_data;
    if (mcu_restart_request) {
        esp_restart();
    }
}

/* Simple handler for getting system handler */
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
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Request accepted");
    mcu_restart_request = true;
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

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    server_context_t *rest_context = (server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

static void ws_free_ctx_handler(void *ctx) {
    ws_sess_context_t *sess_ctx = (ws_sess_context_t *)ctx;
    ws_user_context_t *user_ctx = sess_ctx->user_ctx;

    if (user_ctx->open_fds_length == 0) {
        ESP_LOGE(TAG, "open_fds_length == 0");
        abort();
    }

    // Remove fd from user_ctx
    for (int i = 0; i < user_ctx->open_fds_length - 1; i++) {
        // If not last one, replace with last one
        if (user_ctx->open_fds[i] == sess_ctx->fd)
            user_ctx->open_fds[i] = user_ctx->open_fds[user_ctx->open_fds_length - 1];
    }
    // Remove last one
    user_ctx->open_fds[user_ctx->open_fds_length - 1] = 0;
    user_ctx->open_fds_length--;
    ESP_LOGI(TAG, "Removed %d, WS count: %d", sess_ctx->fd, user_ctx->open_fds_length);

    free(sess_ctx);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    ws_user_context_t *user_ctx = (ws_user_context_t *)req->user_ctx;
    int fd = httpd_req_to_sockfd(req);

    if (req->sess_ctx == NULL) {
        ws_sess_context_t *sess_ctx = (ws_sess_context_t *)calloc(1, sizeof(ws_sess_context_t));
        if (sess_ctx == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for sess_ctx");
            return ESP_ERR_NO_MEM;
        }
        sess_ctx->fd = fd;
        sess_ctx->user_ctx = user_ctx;
        req->sess_ctx = sess_ctx;
        req->free_ctx = ws_free_ctx_handler;

        // Add new fd to user_ctx
        user_ctx->open_fds[user_ctx->open_fds_length] = fd;
        user_ctx->open_fds_length ++;

        ESP_LOGI(TAG, "Added %d, WS count: %d", fd, user_ctx->open_fds_length);
        xSemaphoreGive(user_ctx->opened_sem);
    }

    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "FD=%d, frame len is %d", fd, ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t *)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet type %d message: %s", ws_pkt.type, ws_pkt.payload);
        if (user_ctx->receive_handler != NULL) {
            ret = user_ctx->receive_handler(req, &ws_pkt);
        }
    }

    free(buf);
    return ret;
}
