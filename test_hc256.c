#include "hc256.h"

uint32_t Key[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint32_t IV[8] = {0, 0, 0, 0, 0, 0, 0, 0};

int main() {
    hc256_t * hc256_1 = alloc_hc256();
    hc256_t * hc256_2 = alloc_hc256();

    init(hc256_1, Key, IV);
    init(hc256_2, Key, IV);

    printf("%d, %d\n", keygen(hc256_1), keygen(hc256_2));

    free_hc256(hc256_1);
    free_hc256(hc256_2);

    return 0;
}
