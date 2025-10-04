#include "netlog.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdarg.h>
#include "web_server.h"

static const char *TAG = "netlog";

static QueueHandle_t log_queue = NULL;
static char message_buffer[NETLOG_MAX_MESSAGE_LENGTH];

static int _log_vprintf(const char *fmt, va_list args);

esp_err_t netlog_init()
{
    log_queue = xQueueCreate(NETLOG_QUEUE_LENGTH, sizeof(char **));
    if (log_queue == NULL) {
        ESP_LOGE(TAG, "Cannot create queue");
        return ESP_ERR_NO_MEM;
    }

    /*if (xTaskCreate(netlog_task, "netlog", 2048, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create netlog task");
        vQueueDelete(log_queue);
        return ESP_ERR_NO_MEM;
    }*/

    esp_log_set_vprintf(_log_vprintf);

    return ESP_OK;
}

void netlog_task(void * pvParameters)
{
    void *log_ctx = web_get_log_user_ctx();
    char *print_string = NULL;

    while (1) {

        web_ws_wait_for_client(log_ctx, portMAX_DELAY);

        while (1) {
            if (web_ws_get_num_clients(log_ctx) == 0) break;

            // Block to wait for the next string to print.
            if (xQueueReceive(log_queue, &print_string, portMAX_DELAY) == pdPASS) {

                web_ws_send(log_ctx, (uint8_t *)print_string, strlen(print_string));

                free(print_string);
            }

            if (web_ws_get_num_clients(log_ctx) == 0) break;
        }
    }
}

static int _log_vprintf(const char *fmt, va_list args)
{
    int length = vsnprintf(message_buffer, sizeof(message_buffer), fmt, args) + 1;
    char *print_string = (char *)calloc(1, length);

    if (print_string == NULL) {
        return 0;
    }

    strncpy(print_string, message_buffer, length);

    // Send the string to the logging task for IO.
    if (xQueueSend(log_queue, &print_string, 0) != pdPASS) {
        // The buffer was not sent so must be freed again.
        free(print_string);
    }

    return fwrite(message_buffer, 1, length, stdout);
}
