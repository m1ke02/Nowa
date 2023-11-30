#pragma once

#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t con_init(void);
void con_loop(void);

#ifdef __cplusplus
}
#endif
