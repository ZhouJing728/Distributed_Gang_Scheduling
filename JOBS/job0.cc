#include <stdio.h>
#include <unistd.h>

int main() {
    int seconds = 5;
    int i;

    for (i = 0; i < seconds; ++i) {
        printf("--JOB 0-- Elapsed time: %d seconds\n", i);
        sleep(1); // 等待一秒钟
    }

    printf("--JOB 0-- Loop finished.\n");

    return 0;
}
