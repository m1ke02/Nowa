#include "Dispatcher.h"
#include "esp_log.h"

#define TAG "DISP"

bool Dispatcher::connectNP(uint8_t idx, const BDA& bda) {
	if (idx >= m_activeBDAs.size()) {
		ESP_LOGD(TAG, "Invalid index");
		return false;
	}

	if (isConnected(idx)) {
		ESP_LOGD(TAG, "Index already occupied");
		return false;
	}

	if (m_providerList.count(bda) != 0) {
		m_providerList[bda].setIsActive(true);
	} else {
		NotificationProvider np(bda);
		np.setIsActive(true);
		m_providerList.insert(std::make_pair(bda, np));
	}

	m_activeBDAs[idx] = bda;

	return true;
}

bool Dispatcher::disconnectNP(uint8_t idx, bool deleteAfter) {
	if (idx >= m_activeBDAs.size()) {
		ESP_LOGD(TAG, "Invalid index");
		return false;
	}

	if (m_providerList.count(m_activeBDAs[idx]) == 0) {
		ESP_LOGD(TAG, "Not yet connected");
		return false;
	}

	m_providerList[m_activeBDAs[idx]].setIsActive(false);
	if (deleteAfter) {
		m_providerList.erase(m_activeBDAs[idx]);
	}

	m_activeBDAs[idx].fill(0);

	return true;
}

bool Dispatcher::isConnected(uint8_t idx)
{
	if (idx >= m_activeBDAs.size()) {
		ESP_LOGD(TAG, "Invalid index");
		return false;
	}

	for (size_t i = 0; i < m_activeBDAs[idx].size(); i ++) {
		if (m_activeBDAs[idx][i] != 0) {
			return true;
		}
	}

	return false;
}

NotificationProvider *Dispatcher::getNPById(uint8_t idx) {
	if (idx >= m_activeBDAs.size()) {
		ESP_LOGD(TAG, "Invalid index");
		return nullptr;
	}

	if (m_providerList.count(m_activeBDAs[idx]) == 0) {
		ESP_LOGD(TAG, "NP not connected yet");
		return nullptr;
	}

	return &m_providerList[m_activeBDAs[idx]];
}

NotificationProvider *Dispatcher::getNPByBDA(const BDA& bda)
{
	if (m_providerList.count(bda) == 0) {
		ESP_LOGD(TAG, "NP not present");
		return nullptr;
	}

	return &m_providerList[bda];
}
