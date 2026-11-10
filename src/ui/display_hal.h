#pragma once

#include <stdint.h>

namespace display_hal {
bool initialize(uint8_t rotation = 0);
bool set_rotation(uint8_t rotation);
void poll();
}