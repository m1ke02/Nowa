#ifndef EMCI_CONFIG_H
#define EMCI_CONFIG_H

#include <stdio.h>
#include <stdint.h>
#include "lwip/sockets.h"

#define EMCI_COMMAND_DEL        ';'
#define EMCI_ARG_DEL            ' '
#define EMCI_ENTER_KEY          '\n'
#define EMCI_ENDL               "\r\n"
#define EMCI_ECHO_INPUT         0
#define EMCI_MAX_LINE_LENGTH    32
#define EMCI_MAX_COMMANDS       8
#define EMCI_MAX_ARGS           10    // see "if (!adp)" line inside cmd_help_handler()
#define EMCI_MAX_NAME_LENGTH    12
#define EMCI_PRINTF(...)        { fprintf((FILE *)env->extra, __VA_ARGS__); }
#define EMCI_GET_CHAR(c0)       { c0 = fgetc((FILE *)env->extra); if (c0 == '\r') continue; }
#define EMCI_PUT_CHAR(c0)       { fputc(c0, (FILE *)env->extra); }
#define EMCI_IDLE_TASK()        { }

#endif
