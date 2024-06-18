#include <stdio.h>
#include <unistd.h>

int main() {
    int seconds = 4000;
    int i;

    for (i = 0; i < seconds; ++i) {
        printf("--JOB 4-- Elapsed time: %d seconds\n", i);
        sleep(1); // 等待一秒钟
    }

    printf("--JOB 4-- Loop finished.\n");

    return 0;
}