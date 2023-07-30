#ifndef EMCI_CONFIG_H
#define EMCI_CONFIG_H

#include <stdio.h>
#include <stdint.h>
#include "lwip/sockets.h"

#define EMCI_TX_BUFFER_SIZE     256

extern int emci_socket;
extern char emci_tx_buffer[EMCI_TX_BUFFER_SIZE];

#define EMCI_COMMAND_DEL        ';'
#define EMCI_ARG_DEL            ' '
#define EMCI_ENTER_KEY          '\n'
#define EMCI_ENDL               "\r\n"
#define EMCI_ECHO_INPUT         0
#define EMCI_MAX_LINE_LENGTH    32
#define EMCI_MAX_COMMANDS       8
#define EMCI_MAX_ARGS           10    // see "if (!adp)" line inside cmd_help_handler()
#define EMCI_MAX_NAME_LENGTH    12
#define EMCI_PRINTF(...)        { int len = snprintf(emci_tx_buffer, sizeof(emci_tx_buffer), __VA_ARGS__); send(emci_socket, emci_tx_buffer, len, 0); }
#define EMCI_GET_CHAR(c0)       { if (recv(emci_socket, &c0, 1, 0) < 1) c0 = EOF; else if (c0 == '\r') continue; }
#define EMCI_PUT_CHAR(c0)       { char c1 = c0; send(emci_socket, &c1, 1, 0); }
#define EMCI_IDLE_TASK()        { }

#endif
