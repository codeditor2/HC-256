#include <stdio.h>
#include <stdint.h>
#include <ortp/ortp.h>

struct __hc256_t {
    uint32_t *P;
    uint32_t *Q;
    uint32_t *W;
    unsigned int c;
};

typedef struct __hc256_t hc256_t;

void init(hc256_t *, uint32_t [8], uint32_t [8]);
uint32_t keygen(hc256_t *);
hc256_t *alloc_hc256();
void free_hc256(hc256_t *);
