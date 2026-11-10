#include "ui/display_rotation.h"

#include <stdio.h>

static_assert(display_hal::rotation_madctl(0) == 0x48, "Preserve the accepted orientation");

int main()
{
    const uint8_t expected[] = {0x48, 0x28, 0x88, 0xE8};
    for (unsigned rotation = 0; rotation < 256; ++rotation) {
        if (display_hal::valid_rotation(rotation) != (rotation < 4)) return 1;
        if (rotation < 4) {
            if (display_hal::rotation_madctl(rotation) != expected[rotation]) return 2;
            if ((display_hal::rotation_madctl(rotation) & 0x08) == 0) return 3;
        } else if (display_hal::rotation_madctl(rotation) != 0) {
            return 4;
        }
    }
    puts("PASS: accepted base orientation, four MADCTL mappings, BGR retention and invalid rotation rejection");
    return 0;
}