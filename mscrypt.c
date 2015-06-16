#include <mediastreamer2/msfilter.h>
#include <stdio.h>
#include <stdint.h>
#include <ortp/ortp.h>

#include "hc256.h"

uint32_t Key[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint32_t IV[8] = {0, 0, 0, 0, 0, 0, 0, 0};

typedef enum {
    KEY_INFO = 0xDEADBEEF, AUDIO_FRAME = 0xBEEFDEAD
} FrameType;

typedef struct FrameHeader {
    FrameType type;
} FrameHeader;

typedef struct KeyInfo {
    uint8_t IV0;
    uint8_t IV1;
    uint8_t IV2;
    uint8_t IV3;
    uint8_t IV4;
    uint8_t IV5;
    uint8_t IV6;
    uint8_t IV7;
} KeyInfo;

typedef struct KeyItem {
    uint8_t key;
} KeyItem;

static void msencrypt_init(MSFilter *);
static void msencrypt_uninit(MSFilter *);
static void msencrypt_preprocess(MSFilter *);
static void msencrypt_process(MSFilter *);
static void msencrypt_postprocess(MSFilter *);
static int enc_add_attr(MSFilter *f, void *arg);

typedef struct EncState{
    uint32_t *Key;
    uint32_t *IV;
    uint32_t round;
    uint32_t key;

    uint32_t *table;
    
    hc256_t *hc256;
} EncState;

static MSFilterMethod enc_methods[]={
	{MS_FILTER_ADD_ATTR,	enc_add_attr},
	{0 , NULL}
};

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
    .uninit	=	msencrypt_uninit,
    .methods	=	enc_methods
};

static void msencrypt_init(MSFilter *f)
{
    EncState *s=ms_new0(EncState,1);
    int i;

    s->Key = Key;
    s->IV = IV;

    s->hc256 = alloc_hc256();
    init(s->hc256, s->Key, s->IV);

    s->table = (uint32_t *)malloc(sizeof(uint32_t) * 4096);
    for (i = 0; i < 4096; i++)
	s->table[i] = keygen(s->hc256);

    s->round = 0;
    f->data=s;
    ortp_message("msencrypt_init-------");
}

static void msencrypt_uninit(MSFilter *f)
{
    EncState *s=(EncState*)f->data;

    free(s->table);
    free_hc256(s->hc256);

    ms_free(s);
    ortp_message("msencrypt_uninit");
}

static void msencrypt_preprocess(MSFilter *f)
{
    ortp_message("msencrypt_preprocess");

    EncState *s=(EncState*)f->data;
}

static void msencrypt_process(MSFilter *f)
{
    ortp_message("msencrypt_process");

    mblk_t *im,*om;
    unsigned char * pr;
    EncState *s=(EncState*)f->data;
    FrameHeader fh;
    KeyInfo ki;
    KeyItem km;
    uint32_t key;
    
    s->round++;
    if (s->round%256 != 0) {
	while((im = ms_queue_get(f->inputs[0])) != NULL) {
	    om = NULL;
	    // added by yuan
	    rtp_header_t *rtp=(rtp_header_t*)im->b_rptr;
	    
	    size_t hoff = sizeof(FrameHeader);
	    size_t moff = sizeof(KeyItem);
	    size_t len = im->b_wptr - im->b_rptr + hoff + moff;
	    om = allocb(len, 0);

	    fh.type = AUDIO_FRAME;	    
	    memcpy(om->b_wptr, &fh, hoff);
	    om->b_wptr += hoff;

	    km.key = rand() & 0x00000FFF;
	    memcpy(om->b_wptr, &km, moff);
	    om->b_wptr += moff;

//	    printf("e: %08x, %08x\n", *(im->b_wptr - 1), key);
	    for (pr = im->b_rptr; pr < im->b_wptr; pr++) {
		key = s->table[km.key];
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
	size_t moff = sizeof(KeyInfo);
	size_t len = hoff + moff;
	om = allocb(len, 0);

	fh.type = KEY_INFO;
	memcpy(om->b_wptr, &fh, hoff);
	om->b_wptr += hoff;

	ki.IV0 = rand();
	ki.IV1 = rand();
	ki.IV2 = rand();
	ki.IV3 = rand();
	ki.IV4 = rand();
	ki.IV5 = rand();
	ki.IV6 = rand();
	ki.IV7 = rand();
	IV[0] = s->table[ki.IV0];
	IV[1] = s->table[ki.IV1];
	IV[2] = s->table[ki.IV2];
	IV[3] = s->table[ki.IV3];
	IV[4] = s->table[ki.IV4];
	IV[5] = s->table[ki.IV5];
	IV[6] = s->table[ki.IV6];
	IV[7] = s->table[ki.IV7];
	
	init(s->hc256, s->Key, s->IV);
	for (int i = 0; i < 4096; i++)
	    s->table[i] = keygen(s->hc256);
	
	ortp_message("new key inited");
	memcpy(om->b_wptr, &ki, sizeof(KeyInfo));
	om->b_wptr += moff;
	
	ms_queue_put(f->outputs[0],om);
    }
}

static void msencrypt_postprocess(MSFilter *f)
{
    ortp_message("msencrypt_postprocess");

    EncState *s=(EncState*)f->data;
}

static int enc_add_attr(MSFilter *f, void *arg)
{
	EncState *s=(EncState*)f->data;
	int i;

	if (!s->Key)
		free(s->Key);
	if (!s->IV)
		free(s->Key);
	
	s->Key = (uint32_t *)malloc(sizeof(uint32_t) * 8);
	s->IV = (uint32_t *)malloc(sizeof(uint32_t) * 8);
	
	sscanf((char *)arg, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
		   &(s->Key[0]),
		   &(s->Key[1]),
		   &(s->Key[2]),
		   &(s->Key[3]),
		   &(s->Key[4]),
		   &(s->Key[5]),
		   &(s->Key[6]),
		   &(s->Key[7]),
		   &(s->IV[0]),
		   &(s->IV[1]),
		   &(s->IV[2]),
		   &(s->IV[3]),
		   &(s->IV[4]),
		   &(s->IV[5]),
		   &(s->IV[6]),
		   &(s->IV[7]));
	
	ortp_message("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
				 (s->Key[0]),
				 (s->Key[1]),
				 (s->Key[2]),
				 (s->Key[3]),
				 (s->Key[4]),
				 (s->Key[5]),
				 (s->Key[6]),
				 (s->Key[7]),
				 (s->IV[0]),
				 (s->IV[1]),
				 (s->IV[2]),
				 (s->IV[3]),
				 (s->IV[4]),
				 (s->IV[5]),
				 (s->IV[6]),
				 (s->IV[7]));
	
	if (!s->table)
		free(s->table);
	
	if (!s->hc256)
		free_hc256(s->hc256);
	
	s->hc256 = alloc_hc256();
	init(s->hc256, s->Key, s->IV);
	
	s->table = (uint32_t *)malloc(sizeof(uint32_t) * 4096);
	for (i = 0; i < 4096; i++)
		s->table[i] = keygen(s->hc256);
	
	ortp_message("enc_add_attr");
}

static void msdecrypt_init(MSFilter *);
static void msdecrypt_uninit(MSFilter *);
static void msdecrypt_preprocess(MSFilter *);
static void msdecrypt_process(MSFilter *);
static void msdecrypt_postprocess(MSFilter *);

static int dec_add_attr(MSFilter *f, void *arg);

typedef struct DecState{
    uint32_t *Key;
    uint32_t *IV;
    uint8_t inited;

    uint32_t *table;
    
    hc256_t *hc256;
} DecState;

static MSFilterMethod dec_methods[]={
	{MS_FILTER_ADD_ATTR,	dec_add_attr},
	{0 , NULL}
};

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
    .uninit	=	msdecrypt_uninit,
    .methods=	dec_methods
};

static void msdecrypt_init(MSFilter *f)
{
    DecState *s=ms_new0(DecState,1);
    int i;
    f->data=s;

    s->Key = Key;
    s->IV = IV;

    s->hc256 = alloc_hc256();

    printf("%p\n", s->hc256);
    init(s->hc256, s->Key, s->IV);

    s->table = (uint32_t *)malloc(sizeof(uint32_t) * 4096);
    for (i = 0; i < 4096; i++)
	s->table[i] = keygen(s->hc256);

    s->inited = 0;

    ortp_message("msdecrypt_init");
}

static void msdecrypt_uninit(MSFilter *f)
{
    DecState *s=(DecState*)f->data;

    free(s->table);
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
    ortp_message("msdecrypt_process");

    DecState *s=(DecState*)f->data;
    mblk_t *im,*om;
    FrameHeader fh;
    KeyInfo ki;
    KeyItem km;
    uint32_t key;
    unsigned char * pr;
    int i;

    while((im = ms_queue_get(f->inputs[0])) != NULL) {
	memcpy(&fh, im->b_rptr, sizeof(FrameHeader));
	im->b_rptr += sizeof(FrameHeader);
	if (fh.type == AUDIO_FRAME && s->inited == 1) {
	    om = NULL;
	    om = allocb(im->b_wptr - im->b_rptr, 0);

	    memcpy(&km, im->b_rptr, sizeof(KeyItem));
	    im->b_rptr += sizeof(KeyItem);

//	    printf("d: %08x, %08x\n", *(im->b_wptr - 1), key);
	    for (pr = im->b_rptr; pr < im->b_wptr; pr++) {
		key = s->table[km.key];
		*(om->b_wptr++) = (*pr) ^ key;
	    }
//	    printf("%08x, %08x\n", *(om->b_wptr - 1), key);
	    ms_queue_put(f->outputs[0],om);
	} else if (fh.type == KEY_INFO) {
	    
	    memcpy(&ki, im->b_rptr, sizeof(KeyInfo));

	    s->IV[0] = s->table[ki.IV0];
	    s->IV[1] = s->table[ki.IV1];
	    s->IV[2] = s->table[ki.IV2];
	    s->IV[3] = s->table[ki.IV3];
	    s->IV[4] = s->table[ki.IV4];
	    s->IV[5] = s->table[ki.IV5];
	    s->IV[6] = s->table[ki.IV6];
	    s->IV[7] = s->table[ki.IV7];

	    init(s->hc256, s->Key, s->IV);
	    for (i = 0; i < 4096; i++)
		s->table[i] = keygen(s->hc256);

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
}

static int dec_add_attr(MSFilter *f, void *arg)
{
	DecState *s=(DecState*)f->data;
	int i;

	if (!s->Key)
		free(s->Key);
	if (!s->IV)
		free(s->Key);
	
	s->Key = (uint32_t *)malloc(sizeof(uint32_t) * 8);
	s->IV = (uint32_t *)malloc(sizeof(uint32_t) * 8);
	
	sscanf((char *)arg, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
		   &(s->Key[0]),
		   &(s->Key[1]),
		   &(s->Key[2]),
		   &(s->Key[3]),
		   &(s->Key[4]),
		   &(s->Key[5]),
		   &(s->Key[6]),
		   &(s->Key[7]),
		   &(s->IV[0]),
		   &(s->IV[1]),
		   &(s->IV[2]),
		   &(s->IV[3]),
		   &(s->IV[4]),
		   &(s->IV[5]),
		   &(s->IV[6]),
		   &(s->IV[7]));

	ortp_message("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
				 (s->Key[0]),
				 (s->Key[1]),
				 (s->Key[2]),
				 (s->Key[3]),
				 (s->Key[4]),
				 (s->Key[5]),
				 (s->Key[6]),
				 (s->Key[7]),
				 (s->IV[0]),
				 (s->IV[1]),
				 (s->IV[2]),
				 (s->IV[3]),
				 (s->IV[4]),
				 (s->IV[5]),
				 (s->IV[6]),
				 (s->IV[7]));

	if (!s->table)
		free(s->table);

	if (!s->hc256)
		free_hc256(s->hc256);
	
	s->hc256 = alloc_hc256();
	init(s->hc256, s->Key, s->IV);
	
	s->table = (uint32_t *)malloc(sizeof(uint32_t) * 4096);
	for (i = 0; i < 4096; i++)
		s->table[i] = keygen(s->hc256);

	ortp_message("enc_add_attr");
}

void libmscrypt_init(){
    ms_filter_register(&msencrypt_filter);
    ms_filter_register(&msdecrypt_filter);
    ms_message("libmscrypt plugin loaded");
}
