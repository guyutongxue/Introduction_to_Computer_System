#include <stdio.h>
#include <stdlib.h>
int main(void) {
    unsigned a = 0x11112222;
    unsigned b = 0x33336666;
    void *x = &a;
    void *y = (void*)&b + 2;
    unsigned short p = *(unsigned short*)x;
    unsigned short q = *(unsigned short *)y;
    printf("0x%04x", q + p);

    return EXIT_SUCCESS;
}