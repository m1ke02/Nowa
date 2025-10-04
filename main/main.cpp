#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "netlog.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "protocol_examples_common.h"
#include "tcp_server.h"
#include "ble_ancs.h"
#include "uart_console.h"
#include "web_server.h"
#include "spiffs.h"

#include "lwip/sockets.h"

#include "Dispatcher.h"

#define TAG "MAIN"

void send_info_task(void *pvParameters);

Dispatcher disp;

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "disconnect_handler");
    ESP_ERROR_CHECK(web_stop());
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "connect_handler");
    ESP_ERROR_CHECK(web_start("/spiffs"));
}

extern "C" void app_main(void)
{
    esp_err_t ret;

    esp_log_level_set("nvs", ESP_LOG_INFO);
    esp_log_level_set("ANCS", ESP_LOG_INFO);
    esp_log_level_set("BLEU", ESP_LOG_INFO);
    esp_log_level_set("ANCSU", ESP_LOG_INFO);
    esp_log_level_set("DISP", ESP_LOG_DEBUG);
    esp_log_level_set("CON", ESP_LOG_INFO);

    ESP_ERROR_CHECK(netlog_init());

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //ESP_ERROR_CHECK(disp.initDriver());

    ESP_ERROR_CHECK(con_init());

    ESP_ERROR_CHECK(spiffs_init());

    //con_loop();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(web_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, NULL));

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(netlog_task, "netlog", 2048, NULL, 4, NULL);
    xTaskCreate(send_info_task, "send_info", 4096, NULL, 5, NULL);

#ifdef CONFIG_EXAMPLE_IPV4
    //xTaskCreate(tcp_server_task, "tcp_server", 8192, (void*)AF_INET, 5, NULL);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    //xTaskCreate(tcp_server_task, "tcp_server", 8192, (void*)AF_INET6, 5, NULL);
#endif
}

void send_info_task(void *pvParameters) {
    void *console_ctx = web_get_console_user_ctx();
    char text[128];

    while (1) {
        web_ws_wait_for_client(console_ctx, portMAX_DELAY);
        ESP_LOGI(TAG, "Starting periodic transfer");
        TickType_t prevTime = xTaskGetTickCount();

        while (1) {
            if (web_ws_get_num_clients(console_ctx) == 0) break;

            int len = snprintf(text, sizeof(text), "T=%08lu", prevTime);
            web_ws_send(console_ctx, (uint8_t *)text, len);

            if (web_ws_get_num_clients(console_ctx) == 0) break;
            xTaskDelayUntil(&prevTime, pdMS_TO_TICKS(1000));
        }
        ESP_LOGI(TAG, "Stopping periodic transfer");
    }
}
