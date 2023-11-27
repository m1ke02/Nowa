#pragma once

#include <string>
#include <unordered_map>
#include <queue>

#include "esp_system.h"
#include "ancs.h"

struct Notification {
    std::unordered_map<ble_ancs_c_notif_attr_id_val_t, std::string> attributes;
};

class Dispatcher {

public:
    Dispatcher();
    void AddAttribute();

private:
    std::string m_deviceName;
    std::queue<Notification> m_notifQueue;
};

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t disp_init(void);

#ifdef __cplusplus
}
#endif
