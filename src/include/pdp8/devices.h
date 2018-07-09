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

extern int pdp8_install_mex_tso(pdp8_t *pdp8);

#endif

