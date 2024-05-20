#include<stdio.h>

extern "C"{
    void test();
}

int main()
{
    printf("trying to use test\n");
    test();
    return 0;
}