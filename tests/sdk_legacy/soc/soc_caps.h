#pragma once
// Select the wrapper's legacy branch without replacing the real SDK types.
#include_next "soc/soc_caps.h"
#undef SOC_TWAI_CONTROLLER_NUM
#define SOC_TWAI_CONTROLLER_NUM 1
