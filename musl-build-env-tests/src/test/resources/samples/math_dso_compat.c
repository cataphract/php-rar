#include <math.h>

static double (*const call_ceil)(double) = ceil;
static float (*const call_ceilf)(float) = ceilf;

int check_shared_math(void)
{
    volatile double value = 1.25;
    volatile float value_float = 1.25F;

    return call_ceil(value) == 2.0 && call_ceilf(value_float) == 2.0F;
}
