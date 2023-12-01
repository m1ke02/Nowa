#pragma once

#include <queue>

#include "DispatcherTypes.h"

class NotificationProvider {

public:
	NotificationProvider() = default; // for std::map
	NotificationProvider(BDA bda) : m_bda(bda) { }

	void setIsActive(bool isActive) { m_isActive = isActive; }
    String name(void) { return m_name; }
	void setName(String name) { m_name = name; }
	bool addNotification(const Notification &notif);
	Notification *getLatestNotification(void) { return m_notifQueue.empty() ? nullptr : &m_notifQueue.back(); }
    const std::queue<Notification>& notifications(void) { return m_notifQueue; }

private:
	bool m_isActive;
	String m_name;
	BDA m_bda;
	std::queue<Notification> m_notifQueue;
};
