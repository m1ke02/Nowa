#pragma once

#include <array>
#include <string>
#include <vector>

#include "ble_ancs.h"

using BDA = std::array<uint8_t, 6>;
using String = std::string;
using AttrRequest = std::pair<uint32_t, std::vector<ble_ancs_c_notif_attr_id_val_t>>;

struct Notification {
    String timeStamp;
    String appId;
    String title;
    String subTitle;
    String message;
};
