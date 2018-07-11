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
    void (*rdr_ready)(void *ctx);
    void (*punch)(void *ctx, char ch);
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
extern int pdp8_console_rdr_byte(pdp8_console_t *dev, uint8_t ch);

#endif

