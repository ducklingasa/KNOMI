#pragma once

#include <stdint.h>

namespace display_hal {
constexpr bool valid_rotation(unsigned rotation)
{
    return rotation < 4;
}

constexpr uint8_t rotation_madctl(unsigned rotation)
{
    constexpr uint8_t values[] = {0x48, 0x28, 0x88, 0xE8};
    return valid_rotation(rotation) ? values[rotation] : 0;
}
}