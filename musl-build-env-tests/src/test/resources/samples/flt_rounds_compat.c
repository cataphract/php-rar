#include <fenv.h>
#include <float.h>
#include <stdio.h>

struct rounding_case {
    int environment_mode;
    int flt_rounds_value;
};

int main(void)
{
    int original_mode = fegetround();
    const struct rounding_case cases[] = {
        {FE_TONEAREST, 1},
        {FE_UPWARD, 2},
        {FE_DOWNWARD, 3},
        {FE_TOWARDZERO, 0},
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        if (fesetround(cases[index].environment_mode) != 0) {
            return 1;
        }
        if (FLT_ROUNDS != cases[index].flt_rounds_value) {
            return 2;
        }
    }

    if (fesetround(original_mode) != 0) {
        return 3;
    }

    puts("flt-rounds-ok");
    return 0;
}
