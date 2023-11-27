#include "emci_profile.h"
#include "emci_std_handlers.h"
#include <stdio.h>
#include <inttypes.h>

#include "ancs.h"

// char emci_tx_buffer[EMCI_TX_BUFFER_SIZE];

const emci_command_t cmd_array[] =
{
    {"about", about_handler, "", 0,
    NULL,
    "Display version information", "<strA>\0strB\0<strC>\0strD"},

    {"dl", device_list_handler, "", 0,
    NULL,
    "Print connected device list", NULL},

    {"nl", notification_list_handler, "", 0,
    NULL,
    "Print unread notification list", NULL},

    {"vars", emci_vars_handler, "", 0,
    NULL,
    "Print list of all variables", NULL},

    {"printargs", emci_printargs_handler, "ssssssss", 8,
    NULL,
    "Print all arguments passed", NULL},

    {"help", emci_help_handler, "s", 1,
    NULL,
    "Display this message", "cmd"},

    {"exit", emci_exit_handler, "", 0,
    NULL,
    "Exit command session", NULL}
};

const uint_fast8_t cmd_array_length = (sizeof(cmd_array) / sizeof(emci_command_t));

void emci_prompt(emci_env_t *env)
{
    EMCI_PRINTF("Nowa>");
}

emci_status_t about_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env)
{
    EMCI_PRINTF("Nowa (c) 2023 Home Inc." EMCI_ENDL);
    return (emci_status_t)APP_OK;
}

emci_status_t device_list_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env)
{
    ancs_dump_device_list ((FILE *)env->extra, EMCI_ENDL);
    return EMCI_STATUS_OK;
}

emci_status_t notification_list_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env)
{
    return EMCI_STATUS_OK;
}

const char *emci_app_status_message(emci_status_t status)
{
    return "?";
}
