#pragma once
#include "esp_log.h"
#include <stdbool.h>

extern volatile bool shutdown_requested;
extern volatile bool logger_done;

void request_shutdown(void);