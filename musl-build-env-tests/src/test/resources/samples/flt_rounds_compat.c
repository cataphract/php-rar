#include <dlfcn.h>
#include <fenv.h>
#include <float.h>
#include <stdio.h>

extern const char *gnu_get_libc_version(void) __attribute__((weak));

struct rounding_case {
    int environment_mode;
    int flt_rounds_value;
};

static void *floating_point_environment_handle(int running_on_glibc)
{
    if (!running_on_glibc) {
        return RTLD_DEFAULT;
    }
    return dlopen("libm.so.6", RTLD_LAZY | RTLD_LOCAL);
}

int main(void)
{
    int running_on_glibc = gnu_get_libc_version != NULL;
    void *handle = floating_point_environment_handle(running_on_glibc);
    if (running_on_glibc && handle == NULL) {
        return 10;
    }

    int (*get_rounding_mode)(void) = dlsym(handle, "fegetround");
    int (*set_rounding_mode)(int) = dlsym(handle, "fesetround");
    if (get_rounding_mode == NULL || set_rounding_mode == NULL) {
        return 11;
    }

    int original_mode = get_rounding_mode();
    const struct rounding_case cases[] = {
        {FE_TONEAREST, 1},
        {FE_UPWARD, 2},
        {FE_DOWNWARD, 3},
        {FE_TOWARDZERO, 0},
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        if (set_rounding_mode(cases[index].environment_mode) != 0) {
            return 12;
        }
        if (__flt_rounds() != cases[index].flt_rounds_value ||
            FLT_ROUNDS != cases[index].flt_rounds_value) {
            return 13;
        }
    }

    if (set_rounding_mode(original_mode) != 0) {
        return 14;
    }
    if (running_on_glibc) {
        dlclose(handle);
    }

    puts("flt-rounds-ok");
    return 0;
}
