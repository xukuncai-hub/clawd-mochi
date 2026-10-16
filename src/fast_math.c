#include "fast_math.h"
#include <math.h>

float g_sin_lut[256];

void fast_math_init(void) {
    for (int i = 0; i < 256; i++) {
        g_sin_lut[i] = sinf((float)i * 2.0f * FAST_PI / 256.0f);
    }
}
