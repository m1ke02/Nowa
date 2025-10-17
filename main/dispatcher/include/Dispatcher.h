#pragma once

#include <queue>
#include <map>

#include "esp_system.h"
#include "DispatcherTypes.h"
#include "NotificationProvider.h"

class Dispatcher {

public:
    Dispatcher() = default;
    esp_err_t initDriver(void);
    esp_err_t deinitDriver(void);
    bool connectNP(uint8_t idx, const BDA& bda);
    bool disconnectNP(uint8_t idx, bool deleteAfter = false);
    uint8_t getId(const BDA& bda);
    NotificationProvider *getNPById(uint8_t idx);
    NotificationProvider *getNPByBDA(const BDA& bda);
    std::map<BDA, NotificationProvider>& providers() { return m_providerList; }

    static constexpr uint8_t INVALID_ID = (uint8_t)(-1);

    std::array<std::queue<AttrRequest>, ANCS_PROFILE_NUM> m_attrRequestQueue;
    std::array<Notification, ANCS_PROFILE_NUM> m_notifBuffers;
    std::array<String, ANCS_PROFILE_NUM> m_prevLatestNotifications;

private:
    std::array<BDA, ANCS_PROFILE_NUM> m_activeBDAs;
    std::map<BDA, NotificationProvider> m_providerList;
};
