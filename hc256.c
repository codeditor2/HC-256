#include "hc256.h"

static inline uint32_t right_rotation(uint32_t x, uint32_t n)
{
    return (x >> n) ^ (x << (32-n));
}

static inline uint32_t f1(uint32_t x)
{
    return right_rotation(x, 7) ^ right_rotation(x, 18) ^ (x >> 3);
}

static inline uint32_t f2(uint32_t x)
{
    return right_rotation(x, 17) ^ right_rotation(x, 19) ^ (x >> 10);
}

static inline uint32_t g1(hc256_t *hc256, uint32_t x, uint32_t y)
{
    return (right_rotation(x, 10) ^ right_rotation(y, 23)) +
	hc256->Q[(x^y)%1024];
}

static inline uint32_t g2(hc256_t *hc256, uint32_t x, uint32_t y)
{
    return (right_rotation(x, 10) ^ right_rotation(y, 23)) +
	hc256->P[(x^y)%1024];
}

static inline uint32_t h1(hc256_t *hc256, uint32_t x)
{
    return hc256->Q[x&0xff] + hc256->Q[256+(x>>8&0xff)] +
	hc256->Q[512+(x>>16&0xff)] + hc256->Q[768+(x>>24&0xff)];
}

static inline uint32_t h2(hc256_t *hc256, uint8_t x)
{
    return hc256->P[x&0xff] + hc256->P[256+(x>>8&0xff)] +
	hc256->P[512+(x>>16&0xff)] + hc256->P[768+(x>>24&0xff)];
}

uint32_t keygen(hc256_t *hc256)
{
    unsigned int j;
    uint32_t s;

    j = hc256->c % 1024;

    if (hc256->c % 2048 < 1024) {
	hc256->P[j] = hc256->P[j] + hc256->P[(j-10)%1024] + g1(hc256, hc256->P[(j-3)%1024], hc256->P[(j-1023)%1024]);
	s = h1(hc256, hc256->P[(j-12)%1024]) ^ hc256->P[j];
    } else {
	hc256->Q[j] = hc256->Q[j] + hc256->Q[(j-10)%1024] + g2(hc256, hc256->Q[(j-3)%1024], hc256->Q[(j-1023)%1024]);
	s = h2(hc256, hc256->Q[(j-12)%1024]) ^ hc256->Q[j];
    }
    hc256->c++;
    return s;
}

void init(hc256_t *hc256, uint32_t *K, uint32_t *IV)
{
    uint32_t i;
    hc256->c = 0;
    
    for (i = 0; i < 8; i++) {
	hc256->W[i] = K[i];
    }
    for (i = 8; i < 16; i++) {
	hc256->W[i] = IV[i-8];
    }
    for (i = 16; i < 2560; i++) {
	hc256->W[i] = f2(hc256->W[i-2]) + hc256->W[i-7] + f1(hc256->W[i-15]) +
	    hc256->W[i-16] + i;
    }
    for (i = 0; i < 1024; i++) {
	hc256->P[i] = hc256->W[i+512];
	hc256->Q[i] = hc256->W[i+1536];
    }

    for (i = 0; i < 4096; i++) {
	keygen(hc256);
    }
}

hc256_t *alloc_hc256()
{
    hc256_t *hc256 = (hc256_t *)malloc(sizeof(hc256_t));
    hc256->P = (uint32_t *)malloc(sizeof(uint32_t) * 1024);
    hc256->Q = (uint32_t *)malloc(sizeof(uint32_t) * 1024);
    hc256->W = (uint32_t *)malloc(sizeof(uint32_t) * 2560);
    hc256->c = 0;
    
    return hc256;
}

void free_hc256(hc256_t * hc256)
{
    free(hc256->P);
    free(hc256->Q);
    free(hc256->W);
    free(hc256);
}
