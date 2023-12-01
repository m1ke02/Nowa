#include <stdio.h>
#include <inttypes.h>

#include "emci_profile.h"
#include "emci_std_handlers.h"
#include "Dispatcher.h"
#include "DispatcherUtils.h"

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

extern Dispatcher disp;

void emci_prompt(emci_env_t *env)
{
    EMCI_PRINTF("N>");
}

emci_status_t about_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env)
{
    EMCI_PRINTF("Nowa (c) 2023 Home Inc." EMCI_ENDL);
    return (emci_status_t)APP_OK;
}

emci_status_t device_list_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env)
{
    FILE *f = (FILE *)env->extra;

    int i = 0;
    for (auto p : disp.providers()) {
        BDA bda = p.first;
        String name = p.second.name();
        size_t notifs = p.second.notifications().size();
        uint8_t id = disp.getId(bda);
        if (id != Dispatcher::INVALID_ID) {
            fprintf(f, "[%02d] ", id);
        } else {
            fprintf(f, "[--] ");
        }
        DispatcherUtils::printBDA(f, bda);
        fprintf(f, " '%s' %d Ntf ", p.second.name().c_str(), notifs);
        Notification *latest = p.second.getLatestNotification();
        if (latest) {
            fprintf(f, "Latest %s", latest->timeStamp.c_str());
        }
        fprintf(f, EMCI_ENDL);
        i ++;
    }

    if (i == 0) {
        fprintf(f, "<No devices>" EMCI_ENDL);
    }

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
