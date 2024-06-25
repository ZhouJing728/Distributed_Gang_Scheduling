#include <stdio.h>
#include <unistd.h>
#include"../../include/print_level.h"

int main() {
    int seconds = 2000;
    int i;

    PrintL pLevel;
    for (i = 0; i < seconds; ++i) {
        pLevel.P_JOB("--JOB 2-- Elapsed time: %d seconds\n", i);
        sleep(1); // 等待一秒钟
    }

    pLevel.P_JOB("--JOB 2-- Loop finished.\n");

    return 0;
}