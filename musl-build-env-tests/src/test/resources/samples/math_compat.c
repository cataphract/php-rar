#include <math.h>
#include <stdio.h>

static int check_double(double value, double expected, int negative_zero)
{
    double result = ceil(value);
    return result == expected && (!negative_zero || signbit(result));
}

static int check_float(float value, float expected, int negative_zero)
{
    float result = ceilf(value);
    return result == expected && (!negative_zero || signbit(result));
}

int main(void)
{
    volatile double positive = 1.25;
    volatile double negative = -1.25;
    volatile double negative_fraction = -0.25;
    volatile float positive_float = 1.25F;
    volatile float negative_float = -1.25F;
    volatile float negative_fraction_float = -0.25F;

    if (!check_double(positive, 2.0, 0) ||
        !check_double(negative, -1.0, 0) ||
        !check_double(negative_fraction, -0.0, 1) ||
        !check_float(positive_float, 2.0F, 0) ||
        !check_float(negative_float, -1.0F, 0) ||
        !check_float(negative_fraction_float, -0.0F, 1)) {
        fputs("ceil/ceilf returned an incorrect result\n", stderr);
        return 1;
    }

    puts("ceil ceilf ok");
    return 0;
}
