#include <stdio.h>
int main(void) {
    double pi = 0;
    int k;
    for (k = 0; k >= 0; k++) {
        pi += (k & 1 ? -1 : 1) / (double)(2 * k + 1);
    }
    pi *= 4;
    printf("%f\n", pi);
}