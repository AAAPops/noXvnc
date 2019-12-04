#include <stdio.h>

/* Convert memory dump to hex  */
void mem2hex(int cond, unsigned char *str, int len, int colum_counts) {
    int x;

    if (cond) {
        printf("  ");

        for (x = 0; x < len; x++) {
            if (x > 0 && x % colum_counts == 0)
                printf("\n  ");
            printf("%02x ", str[x]);
        }
        printf("\n");
    }
}