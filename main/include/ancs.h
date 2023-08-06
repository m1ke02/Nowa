#pragma once

#include <stdio.h>
#include "esp_system.h"

esp_err_t ancs_init(void);
void ancs_dump_device_list(FILE *stream, const char *endl);
