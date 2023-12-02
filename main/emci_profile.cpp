#include <stdio.h>
#include <inttypes.h>

#include "emci_profile.h"
#include "emci_std_handlers.h"
#include "esp_system.h"
#include "Dispatcher.h"
#include "DispatcherUtils.h"

const emci_command_t cmd_array[] =
{
    {"about", about_handler, "", 0,
    NULL,
    "Display version information", "<strA>\0strB\0<strC>\0strD"},

    {"dl", device_list_handler, "", 0,
    NULL,
    "Print device list", NULL},

    {"nl", notification_list_handler, "u", 0,
    NULL,
    "Print specified device notifications", "DeviceNum"},

    {"reset", reset_handler, "", 0,
    NULL,
    "Reset MCU", NULL},

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

    fprintf(f, " Num | Stat |       BDA       |  Device Name | Ntfs | Latest Timestamp " EMCI_ENDL);
    fprintf(f, "-----+------+-----------------+--------------+------+------------------" EMCI_ENDL);
    int i = 0;
    for (auto p : disp.providers()) {
        i ++;
        BDA bda = p.first;
        String name = p.second.name();
        size_t notifs = p.second.notifications().size();
        uint8_t id = disp.getId(bda);
        fprintf(f, " %03d |", i);
        if (id != Dispatcher::INVALID_ID) {
            fprintf(f, " On/%d |", id);
        } else {
            fprintf(f, "  Off |");
        }
        DispatcherUtils::printBDA(f, bda);
        fprintf(f, "| %12s | %4d |", p.second.name().c_str(), notifs);
        Notification *latest = p.second.getLatestNotification();
        if (latest) {
            fprintf(f, " %s", latest->timeStamp.c_str());
        }
        fprintf(f, EMCI_ENDL);
    }

    if (i == 0) {
        fprintf(f, "<No devices>" EMCI_ENDL);
    }

    return EMCI_STATUS_OK;
}

emci_status_t notification_list_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env)
{
    FILE *f = (FILE *)env->extra;
    int devNum = argv[1].u;

    if (devNum <= 0) {
        env->resp.param = 1;
        return EMCI_STATUS_ARG_TOO_LOW;
    } else if (devNum > disp.providers().size()) {
        env->resp.param = 1;
        return EMCI_STATUS_ARG_TOO_HIGH;
    }

    int i = 0;
    const NotificationProvider *np = nullptr;
    for (const auto& p : disp.providers()) {
        i ++;
        if (i == devNum) {
            np = &p.second;
            break;
        }
    }

    i = 0;
    for (const auto& n : np->notifications()) {
        i ++;
        fprintf(f, "---------------- Notification %d ----------------" EMCI_ENDL, i);
        fprintf(f, "Date/Time : %s" EMCI_ENDL, n.timeStamp.c_str());
        fprintf(f, "AppId     : %s" EMCI_ENDL, n.appId.c_str());
        fprintf(f, "Title     : %s" EMCI_ENDL, n.title.c_str());
        if (!n.subTitle.empty()) {
            fprintf(f, "Subtitle  : %s" EMCI_ENDL, n.subTitle.c_str());
        }
        fprintf(f, "Message   : %s" EMCI_ENDL EMCI_ENDL, n.message.c_str());
    }

    if (i == 0) {
        fprintf(f, "<No notifications>" EMCI_ENDL);
    }

    return EMCI_STATUS_OK;
}

emci_status_t reset_handler(uint8_t argc, emci_arg_t *argv, emci_env_t *env)
{
    esp_restart();
    return EMCI_STATUS_OK;
}

const char *emci_app_status_message(emci_status_t status)
{
    return "?";
}
