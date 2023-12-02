#include "NotificationProvider.h"
#include "esp_log.h"

#define TAG "DISP"

bool NotificationProvider::addNotification(const Notification &notif) {
	if (!m_isActive) {
		ESP_LOGD(TAG, "Device not active");
		return false;
	}

	m_notifQueue.push_back(notif);
	return true;
}
