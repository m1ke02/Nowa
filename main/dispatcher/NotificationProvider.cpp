#include "NotificationProvider.h"
#include "esp_log.h"

#define TAG "DISP"

bool NotificationProvider::addNotification(Notification &notif) {
	if (!m_isActive) {
		ESP_LOGD(TAG, "Device not active");
		return false;
	}

	Notification *latest = getLatestNotification();
	if (latest != nullptr && notif.timeStamp.compare(latest->timeStamp) <= 0) {
		ESP_LOGD(TAG, "Notification outdated");
		return false;
	}

	m_notifQueue.push(notif);
	return true;
};
