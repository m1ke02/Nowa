#ifndef EMCI_PROFILE_H
#define EMCI_PROFILE_H

#include "emci_parser.h"

typedef enum
{
    APP_OK = EMCI_STATUS_OK,
    APP_IO_ERROR = EMCI_STATUS_APP_ERROR_START,
    APP_FREQ_ERROR
} app_status_t;

void emci_prompt(emci_env_t *env);
emci_status_t about_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env);
emci_status_t device_list_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env);
emci_status_t notification_list_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env);
const char *emci_app_status_message(emci_status_t status);

#endif
