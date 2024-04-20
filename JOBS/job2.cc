#include <stdio.h>
#include <unistd.h>

int main() {
    int seconds = 2000;
    int i;

    for (i = 0; i < seconds; ++i) {
        printf("--JOB 2-- Elapsed time: %d seconds\n", i);
        sleep(1); // 等待一秒钟
    }

    printf("--JOB 2-- Loop finished.\n");

    return 0;
}