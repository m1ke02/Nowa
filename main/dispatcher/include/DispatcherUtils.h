#pragma once

#include <stdio.h>
#include "Dispatcher.h"

class DispatcherUtils {

public:
    static void printNotif(ble_ancs_c_evt_notif_t *p_notif);
    static void printBDA(FILE *stream, BDA bda);
    static void printNotifAttr(uint32_t uid, ble_ancs_c_attr_t *p_attr);
};
