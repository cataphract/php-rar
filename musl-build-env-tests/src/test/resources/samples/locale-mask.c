#include <errno.h>
#include <locale.h>
#include <stdio.h>

int main(void)
{
    errno = 0;
    locale_t locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    if (locale == (locale_t)0) {
        perror("newlocale(LC_ALL_MASK, C)");
        return 1;
    }

    locale_t previous = uselocale(locale);
    if (previous == (locale_t)0) {
        perror("uselocale");
        freelocale(locale);
        return 2;
    }

    if (uselocale(previous) == (locale_t)0) {
        perror("restore locale");
        freelocale(locale);
        return 3;
    }

    freelocale(locale);
    puts("locale mask accepted");
    return 0;
}
