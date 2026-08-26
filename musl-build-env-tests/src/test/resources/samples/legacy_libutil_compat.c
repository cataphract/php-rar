#include <pty.h>
#include <stdio.h>

int main(void)
{
    int terminal_master;
    int terminal_slave;
    if (openpty(&terminal_master, &terminal_slave, NULL, NULL, NULL) != 0) {
        return 1;
    }
    puts("libutil ok");
    return 0;
}
