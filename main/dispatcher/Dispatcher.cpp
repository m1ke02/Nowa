#include "Dispatcher.h"
#include "esp_log.h"

#define TAG "DISP"

static const BDA m_zeroBDA { 0, 0, 0, 0, 0, 0 };

bool Dispatcher::connectNP(uint8_t idx, const BDA& bda) {
	if (idx >= m_activeBDAs.size()) {
		ESP_LOGE(TAG, "Invalid index");
		return false;
	}

	if (m_activeBDAs[idx] != m_zeroBDA) {
		ESP_LOGE(TAG, "Index already occupied");
		return false;
	}

    if (getId(bda) != Dispatcher::INVALID_ID) {
        ESP_LOGE(TAG, "Already connected");
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
		ESP_LOGE(TAG, "Invalid index");
		return false;
	}

	if (m_providerList.count(m_activeBDAs[idx]) == 0) {
		ESP_LOGE(TAG, "Not yet connected");
		return false;
	}

	m_providerList[m_activeBDAs[idx]].setIsActive(false);
	if (deleteAfter) {
		m_providerList.erase(m_activeBDAs[idx]);
	}

	m_activeBDAs[idx].fill(0);

	return true;
}

uint8_t Dispatcher::getId(const BDA& bda)
{
	for (size_t i = 0; i < m_activeBDAs.size(); i ++) {
		if (m_activeBDAs[i] == bda) {
			return (uint8_t)i;
		}
	}

	return INVALID_ID;
}

NotificationProvider *Dispatcher::getNPById(uint8_t idx) {
	if (idx >= m_activeBDAs.size()) {
		ESP_LOGE(TAG, "Invalid index");
		return nullptr;
	}

	if (m_providerList.count(m_activeBDAs[idx]) == 0) {
		ESP_LOGE(TAG, "NP not connected yet");
		return nullptr;
	}

	return &m_providerList[m_activeBDAs[idx]];
}

NotificationProvider *Dispatcher::getNPByBDA(const BDA& bda)
{
	if (m_providerList.count(bda) == 0) {
		ESP_LOGE(TAG, "NP not present");
		return nullptr;
	}

	return &m_providerList[bda];
}
