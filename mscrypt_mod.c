#include <mediastreamer2/msfilter.h>
#include <stdio.h>
#include <stdint.h>
#include <ortp/ortp.h>

#include "hc256.h"

static void msencrypt_init(MSFilter *);
static void msencrypt_uninit(MSFilter *);
static void msencrypt_preprocess(MSFilter *);
static void msencrypt_process(MSFilter *);
static void msencrypt_postprocess(MSFilter *);

uint32_t Key[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint32_t IV[8] = {0, 0, 0, 0, 0, 0, 0, 0};

typedef enum {
    KEY_INFO = 0xDEADBEEF, AUDIO_FRAME = 0xBEEFDEAD
} FrameType;

typedef struct FrameHeader {
    FrameType type;
} FrameHeader;

typedef struct KeyInfo {
    uint32_t IV0;
    uint32_t IV1;
    uint32_t IV2;
    uint32_t IV3;
    uint32_t IV4;
    uint32_t IV5;
    uint32_t IV6;
    uint32_t IV7;
} KeyInfo;

typedef struct EncState{
    uint32_t *Key;
    uint32_t *IV;
    uint32_t round;
    uint32_t key;
    
    hc256_t *hc256;
} EncState;

static MSFilterDesc msencrypt_filter={
    .id		=	MS_FILTER_PLUGIN_ID,
    .name	=	"Encrypt",
    .text	=	"HC256 Stream EnCrypto",
    .category	=	MS_FILTER_OTHER,
    .ninputs	=	1,
    .noutputs	=	1,
    .init	=	msencrypt_init,
    .preprocess	=	msencrypt_preprocess,
    .process	=	msencrypt_process,
    .postprocess=	msencrypt_postprocess,
    .uninit	=	msencrypt_uninit
};

static void msencrypt_init(MSFilter *f)
{
    EncState *s=ms_new0(EncState,1);
    f->data=s;

    s->Key = Key;
    s->IV = IV;

    s->hc256 = alloc_hc256();
     init(s->hc256, s->Key, s->IV);

    s->round = 0;
    ortp_message("msencrypt_init");
}

static void msencrypt_uninit(MSFilter *f)
{
    ortp_message("msencrypt_uninit");

    EncState *s=(EncState*)f->data;

    free_hc256(s->hc256);

    ms_free(s);
}

static void msencrypt_preprocess(MSFilter *f)
{
    ortp_message("msencrypt_preprocess");

    EncState *s=(EncState*)f->data;
}

static void msencrypt_process(MSFilter *f)
{
    mblk_t *im,*om;
    unsigned char * pr;
    EncState *s=(EncState*)f->data;
    FrameHeader fh;
    KeyInfo ki;
    uint32_t key;
    
    s->round++;
    if (s->round%64 != 0) {
	while((im = ms_queue_get(f->inputs[0])) != NULL) {
	    om = NULL;
	    size_t hoff = sizeof(FrameHeader);
	    size_t len = im->b_wptr - im->b_rptr + hoff;
	    om = allocb(len, 0);
	    fh.type = AUDIO_FRAME;
	    key = keygen(s->hc256);
	    memcpy(om->b_wptr, &fh, hoff);
	    om->b_wptr += hoff;
//	    printf("%08x, %08x\n", *(im->b_wptr - 1), key);
	    for (pr = im->b_rptr; pr < im->b_wptr; pr++) {
		*(om->b_wptr++) = (*pr) ^ key;
	    }
//	    printf("%08x, %08x\n", *(om->b_wptr - 1), key);
	    ms_queue_put(f->outputs[0],om);
	    freemsg(im);
	}
    } else {
	while((im = ms_queue_get(f->inputs[0])) != NULL) {
	    freemsg(im);
	}
	size_t hoff = sizeof(FrameHeader);
	size_t len = sizeof(KeyInfo) + hoff;
	om = allocb(len, 0);
	fh.type = KEY_INFO;
	memcpy(om->b_wptr, &fh, hoff);

	ki.IV0 = keygen(s->hc256);
	ki.IV1 = keygen(s->hc256);
	ki.IV2 = keygen(s->hc256);
	ki.IV3 = keygen(s->hc256);
	ki.IV4 = keygen(s->hc256);
	ki.IV5 = keygen(s->hc256);
	ki.IV6 = keygen(s->hc256);
	ki.IV7 = keygen(s->hc256);
	IV[0] = ki.IV0;
	IV[1] = ki.IV1;
	IV[2] = ki.IV2;
	IV[3] = ki.IV3;
	IV[4] = ki.IV4;
	IV[5] = ki.IV5;
	IV[6] = ki.IV6;
	IV[7] = ki.IV7;

	init(s->hc256, s->Key, s->IV);
//	ortp_message("new key inited");
	om->b_wptr += hoff;
	memcpy(om->b_wptr, &ki, sizeof(KeyInfo));
	om->b_wptr += sizeof(KeyInfo);
	
	ms_queue_put(f->outputs[0],om);
    }
}

static void msencrypt_postprocess(MSFilter *f)
{
    ortp_message("msencrypt_postprocess");

    EncState *s=(EncState*)f->data;
}

static void msdecrypt_init(MSFilter *);
static void msdecrypt_uninit(MSFilter *);
static void msdecrypt_preprocess(MSFilter *);
static void msdecrypt_process(MSFilter *);
static void msdecrypt_postprocess(MSFilter *);

typedef struct DecState{
    void *dec;
    uint32_t *Key;
    uint32_t *IV;
    uint8_t inited;

    hc256_t *hc256;
} DecState;

static MSFilterDesc msdecrypt_filter={
    .id		=	MS_FILTER_PLUGIN_ID,
    .name	=	"Decrypt",
    .text	=	"HC256 Stream DeCrypto",
    .category	=	MS_FILTER_OTHER,
    .ninputs	=	1,
    .noutputs	=	1,
    .init	=	msdecrypt_init,
    .preprocess	=	msdecrypt_preprocess,
    .process	=	msdecrypt_process,
    .postprocess=	msdecrypt_postprocess,
    .uninit	=	msdecrypt_uninit
};

static void msdecrypt_init(MSFilter *f)
{
    ortp_message("msdecrypt_init");

    DecState *s=ms_new0(DecState,1);
    f->data=s;

    s->Key = Key;
    s->IV = IV;

    s->hc256 = alloc_hc256();

    printf("%p\n", s->hc256);
    init(s->hc256, s->Key, s->IV);

    s->inited = 0;
}

static void msdecrypt_uninit(MSFilter *f)
{
    DecState *s=(DecState*)f->data;
    free(s->hc256);
    ms_free(s);

    ortp_message("msdecrypt_uninit");
}

static void msdecrypt_preprocess(MSFilter *f)
{
    ortp_message("msdecrypt_preprocess");

    DecState *s=(DecState*)f->data;
}

static void msdecrypt_process(MSFilter *f)
{
    DecState *s=(DecState*)f->data;
    mblk_t *im,*om;
    int err;
    FrameHeader fh;
    uint32_t key;
    unsigned char * pr;

    while((im = ms_queue_get(f->inputs[0])) != NULL) {
	memcpy(&fh, im->b_rptr, sizeof(FrameHeader));
	im->b_rptr += sizeof(FrameHeader);
	if (fh.type == AUDIO_FRAME && s->inited == 1) {
	    om = NULL;
	    om = allocb(im->b_wptr - im->b_rptr, 0);
	    key = keygen(s->hc256);
//	    printf("%08x, %08x\n", *(im->b_wptr - 1), key);
	    for (pr = im->b_rptr; pr < im->b_wptr; pr++) {
		*(om->b_wptr++) = (*pr) ^ key;
	    }
//	    printf("%08x, %08x\n", *(om->b_wptr - 1), key);
	    ms_queue_put(f->outputs[0],om);
	} else if (fh.type == KEY_INFO) {
	    KeyInfo ki;
	    memcpy(&ki, im->b_rptr, sizeof(KeyInfo));

	    s->IV[0] = ki.IV0;
	    s->IV[1] = ki.IV1;
	    s->IV[2] = ki.IV2;
	    s->IV[3] = ki.IV3;
	    s->IV[4] = ki.IV4;
	    s->IV[5] = ki.IV5;
	    s->IV[6] = ki.IV6;
	    s->IV[7] = ki.IV7;

	    init(s->hc256, s->Key, s->IV);
	    s->inited = 1;
//	    ortp_message("new key updated");
	} else {
//	    ortp_message("package discarded");
	}
	freemsg(im);
    }
}

static void msdecrypt_postprocess(MSFilter *f)
{
    ortp_message("msdecrypt_postprocess");

    DecState *s=(DecState*)f->data;
    s->dec=NULL;
}

void libmscrypt_init(){
    ms_filter_register(&msencrypt_filter);
    ms_filter_register(&msdecrypt_filter);
    ms_message("libmscrypt plugin loaded");
}