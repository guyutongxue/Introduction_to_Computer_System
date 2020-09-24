#include <stdio.h>
int main()
{
    char a[12] = "11224455";
    char b[12] = "11445577";
    void *x = &a;
    void *y = 2 + (void *)&b;
    unsigned short p = *(unsigned short *)x;
    unsigned short q = *(unsigned short *)y;
    printf("0x%04x", q - p);
}