#include <stdio.h>

#include "uart_console.h"
#include "emci_parser.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"

#define TAG "CON"

static ssize_t console_vfs_write(int fd, const void * data, size_t size);
static int console_vfs_open(const char * path, int flags, int mode);
static int console_vfs_close(int fd);
static ssize_t console_vfs_read(int fd, void * dst, size_t size);

static const esp_vfs_t console_vfs = {
    .flags = ESP_VFS_FLAG_DEFAULT,
    .write = &console_vfs_write,
    .open = &console_vfs_open,
    .close = &console_vfs_close,
    .read = &console_vfs_read,
};

static emci_env_t emci_env;

esp_err_t con_init(void) {
    esp_err_t ret;

    /* Install UART driver for interrupt-driven reads and writes */
    ret = uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
    if (ret) {
        ESP_LOGE(TAG, "%s: uart_driver_install failed, error code = %x", __func__, ret);
        return ret;
    }
    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    // Create console IO stream and store into emci_env instance
    ret = esp_vfs_register("/con", &console_vfs, NULL);
    if (ret) {
        ESP_LOGE(TAG, "%s: uart_driver_install failed, error code = %x", __func__, ret);
        return ret;
    }
    FILE *stream = fopen("/con", "r+");
    setvbuf(stream, NULL, _IONBF, 0);
    emci_env.extra = stream;

    return ESP_OK;
}

void con_loop(void) {
    while (1) {
        emci_main_loop(&emci_env);
    }
}

static int console_vfs_open(const char *path, int flags, int mode) {
    ESP_LOGV(TAG, "console_vfs_open");
    return 0;
}

static int console_vfs_close(int fd) {
    return 0;
}

static ssize_t console_vfs_write(int fd, const void *data, size_t size) {
    ESP_LOGV(TAG, "vfs write %d", size);
    return write(STDOUT_FILENO, data, size);
}

static ssize_t console_vfs_read(int fd, void *dst, size_t size) {
    int len = read(STDIN_FILENO, dst, size);
    ESP_LOGV(TAG, "vfs read %d", len);
    return len;
}
