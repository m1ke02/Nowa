#pragma once

#include <queue>
#include <map>

#include "esp_system.h"
#include "DispatcherTypes.h"
#include "NotificationProvider.h"

class Dispatcher {

public:
    Dispatcher() = default;
    esp_err_t initDriver();
    bool connectNP(uint8_t idx, BDA bda);
    bool disconnectNP(uint8_t idx, bool deleteAfter = false);
    bool isConnected(uint8_t idx);
    NotificationProvider *getNPById(uint8_t idx);
    NotificationProvider *getNPByBDA(BDA bda);

    std::array<std::queue<AttrRequest>, ANCS_PROFILE_NUM> m_attrRequestQueue;
    std::array<bool, ANCS_PROFILE_NUM> m_attrRequestInProgress;
    std::array<Notification, ANCS_PROFILE_NUM> m_notifBuffers;

private:
    std::array<BDA, ANCS_PROFILE_NUM> m_activeBDAs;
    std::map<BDA, NotificationProvider> m_providerList;
};
