#include "utils.h"

#include <stdio.h>
void print_u8(const uint8_t *u8array, uint32_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%.2x ", u8array[i]);
    }
    printf("\n");
}