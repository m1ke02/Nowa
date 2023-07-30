#include "emci_profile.h"
#include "emci_std_handlers.h"
#include <stdio.h>
#include <inttypes.h>

char emci_tx_buffer[EMCI_TX_BUFFER_SIZE];
int emci_socket = -1;

const emci_command_t cmd_array[] =
{
    {"about", about_handler, "", 0,
    NULL,
    "Display version information", "<strA>\0strB\0<strC>\0strD"},

    {"vars", emci_vars_handler, "", 0,
    NULL,
    "Print list of all variables", NULL},

    {"printargs", emci_printargs_handler, "ssssssss", 8,
    NULL,
    "Print all arguments passed", NULL},

    {"help", emci_help_handler, "s", 1,
    NULL,
    "Display this message", "cmd"}
};

const uint_fast8_t cmd_array_length = (sizeof(cmd_array) / sizeof(emci_command_t));

void emci_prompt(void)
{
    EMCI_PRINTF("Nowa>");
}

emci_status_t about_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env)
{
    EMCI_PRINTF("Nowa (c) 2023 Home Inc." EMCI_ENDL);
    return (emci_status_t)APP_OK;
}

const char *emci_app_status_message(emci_status_t status)
{
    return "?";
}
