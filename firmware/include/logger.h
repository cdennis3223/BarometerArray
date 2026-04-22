#pragma once

#include "SD.h"
#include "collate.h"

extern volatile bool sampling_enabled;

void logger_task(void *args);
