#include <stdio.h>
#include <unistd.h>
#include"../../include/print_level.h"

int main() {
    int seconds = 4000;
    int i;

    PrintL pLevel;
    for (i = 0; i < seconds; ++i) {
        pLevel.P_JOB("--JOB 4-- Elapsed time: %d seconds\n", i);
        sleep(1); 
    }

    pLevel.P_JOB("--JOB 4-- Loop finished.\n");

    return 0;
}