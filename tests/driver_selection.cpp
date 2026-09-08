#include "ESP32-TWAI-CAN.hpp"

// Both fixtures deliberately expose IDF 5.5+ headers. Header availability alone
// must never switch S3/older single-controller chips onto the new driver.
#if !__has_include(<esp_twai_onchip.h>)
#error Test requires new-driver headers to be available
#endif
static_assert(SOC_TWAI_CONTROLLER_NUM == TEST_TWAI_CONTROLLERS, "unexpected SDK controller count");
#if TEST_TWAI_CONTROLLERS == 1
#ifdef TWAI_CAN_NEW_DRIVER
#error Single-controller chips must use the legacy TWAI driver
#endif
bool twaiLegacySnapshotCompileCheck(TwaiCAN& can, twai_status_info_t* out) {
    return can.getStatus(out);
}
#elif TEST_TWAI_CONTROLLERS == 2
#ifndef TWAI_CAN_NEW_DRIVER
#error Dual-controller chips with IDF 5.5 headers must use the new TWAI driver
#endif
#else
#error Unsupported driver-selection test fixture
#endif
