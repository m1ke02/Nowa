#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "tcp_server.h"
#include "ble_ancs.h"
#include "uart_console.h"

#include "lwip/sockets.h"

#include "Dispatcher.h"

#define TAG "MAIN"

Dispatcher disp;

extern "C" void app_main(void)
{
    esp_err_t ret;

    esp_log_level_set("nvs", ESP_LOG_VERBOSE);
    esp_log_level_set("ANCS", ESP_LOG_DEBUG);
    esp_log_level_set("BLEU", ESP_LOG_INFO);
    esp_log_level_set("ANCSU", ESP_LOG_DEBUG);
    esp_log_level_set("DISP", ESP_LOG_INFO);
    esp_log_level_set("CON", ESP_LOG_INFO);

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //ESP_ERROR_CHECK(disp.initDriver());

    ESP_ERROR_CHECK(con_init());

    con_loop();

    //ESP_ERROR_CHECK(esp_netif_init());

    //ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    //ESP_ERROR_CHECK(example_connect());

#ifdef CONFIG_EXAMPLE_IPV4
    //xTaskCreate(tcp_server_task, "tcp_server", 8192, (void*)AF_INET, 5, NULL);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    //xTaskCreate(tcp_server_task, "tcp_server", 8192, (void*)AF_INET6, 5, NULL);
#endif
}
