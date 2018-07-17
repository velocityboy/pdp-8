#ifndef _PDP8_DEVICES_H_
#define _PDP8_DEVICES_H_

#include "pdp8/emulator.h"

/* KM8-E: Memory Extension and Time Share option */

#define MEX_FIELD_OF(op) (((op) >> 3) & 07)
#define MEX_FIELD_OP(op) ((op) & 07707)
#define MEX_MAKE_OP(op, field) ((op) | ((field) << 3))

#define MEX_CDF 06201           /* CDF  62N1 where N is the field */
#define MEX_CIF 06202           /* CIF  62N2 where N is the field */
#define MEX_CDF_CIF 06203       /* both 62N3 where N is the field */

#define MEX_RDF 06214
#define MEX_RIF 06224
#define MEX_RIB 06234
#define MEX_RMF 06244

extern int pdp8_install_mex_tso(pdp8_t *pdp8);

typedef struct pdp8_console_callbacks_t pdp8_console_callbacks_t;

struct pdp8_console_callbacks_t {
    void *ctx;
    void (*free)(void *ctx);
    void (*kbd_ready)(void *ctx);
    void (*print)(void *ctx, char ch);
};

typedef struct pdp8_console_t pdp8_console_t;

#define KCF 06030 /* LC8-E */
#define KSF 06031 /* both */
#define KCC 06032 /* KL8-E */
#define KRS 06034 /* both */
#define KIE 06035 /* LC8-E */
#define KRB 06036 /* both */

#define TFL 06040 /* LC8-E */
#define TSF 06041 /* both */
#define TCF 06042 /* both */
#define TPC 06044 /* both */
#define TSK 06045 /* LC8-E */
#define TLS 06046 /* both */

extern int pdp8_install_console(pdp8_t *pdp8, pdp8_console_callbacks_t *callbacks, pdp8_console_t **console);
extern int pdp8_console_kbd_byte(pdp8_console_t *dev, uint8_t ch);

typedef struct pdp8_punch_callbacks_t pdp8_punch_callbacks_t;

struct pdp8_punch_callbacks_t {
    void *ctx;
    void (*free)(void *ctx);
    void (*rdr_ready)(void *ctx);
    void (*punch)(void *ctx, uint8_t ch);
};

typedef struct pdp8_punch_t pdp8_punch_t;

extern int pdp8_install_punch(pdp8_t *pdp8, pdp8_punch_callbacks_t *callbacks, pdp8_punch_t **punch);
extern int pdp8_punch_rdr_byte(pdp8_punch_t *dev, uint8_t ch);

/* 750C/PR8-E */ 

#define RPE 06010 /* PR8-E */
#define RSF 06011 /* both */
#define RRB 06012 /* both */
#define RFC 06014 /* both */
#define RRB_RFC 06016 /* PR8-E */

/* 75E/PP8-E */ 
#define PCE 06020 /* PP8-E */
#define PSF 06021 /* both */
#define PCF 06022 /* both */
#define PPC 06024 /* both */
#define PLS 06026 /* PP8-E */

typedef struct pdp8_rk8e_t pdp8_rk8e_t;
typedef struct pdp8_rk8e_callbacks_t pdp8_rk8e_callbacks_t;

struct pdp8_rk8e_callbacks_t {
    void *ctx;
    int (*has_media)(void *ctx, int slot);
    int (*read)(void *ctx, int slot, uint32_t offset, uint32_t bytes, uint8_t *buffer);
    int (*write)(void *ctx, int slot, uint32_t offset, uint32_t bytes, uint8_t *buffer);
};

extern int pdp8_install_rk8e(pdp8_t *pdp8, pdp8_rk8e_callbacks_t *callbacks, pdp8_rk8e_t **rk8e);
extern void pdp8_rk8e_set_mounted(pdp8_rk8e_t *rk, int slot, int mounted);

/* RK8-E */
#define DSKP 06741 
#define DCLR 06742
#define DLAG 06743 
#define DLCA 06744
#define DRST 06745
#define DLDC 06746

#endif

