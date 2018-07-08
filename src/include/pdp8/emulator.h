#ifndef _PDP8_EMULATOR_H_
#define _PDP8_EMULATOR_H_

#include <stdint.h>

#include "pdp8/defines.h"

/*
 * This is not every model of the PDP-8, just the models which have architectural 
 * differences. The LINC-8 is identical to the straight eight, and the PDP-8/F and
 * PDP-8/M contain the same CPU as the PDP-8/E.
 */
typedef enum {
    PDP8,
    PDP8_S,
    PDP8_I,
    PDP8_L,
    PDP8_E,
    PDP8_A
} pdp8_model_t;

/* The behavior of coming left and right rotates in the same microinstruction 
 * varied a lot between models.
 */
typedef enum {
    PDP8_RARL_UNDEFINED,        /* garbage out */
    PDP8_RARL_AND,              /* result is the AND of both rotates */
    PDP8_RARL_AND_INSTR,        /* result is the AND of AC and the instruction word */
} pdp8_rar_ral_t;

typedef enum {
    PDP8_RTRL_UNDEFINED,        /* garbage out */
    PDP8_RTRL_AND,              /* result is the AND of both rotates */
    PDP8_RTRL_PAGE_INSTR,       /* result is a combination of the current page and the instruction addr field */
    PDP8_RTRL_NEXT_ADDR,        /* loads the next address */
} pdp8_rtr_rtl_t;

/* behavior quirks/new features per CPU model 
 */
#define PDP8_IAC_ROTS_SUPPORTED   00000001 /* group 1 rotation + IAC is supported */
#define PDP8_IOT0_FULL_INTR_SET   00000002 /* On early models, only ION and IOFF are supported in IOT 0 */
#define PDP8_CLA_NMI_HANGS        00000004 /* CLA+NMI hangs on non-zero AC */
#define PDP8_CMA_ROTS_SUPPORTED   00000010 /* CMA does works with rotates */
#define PDP8_BSW_SUPPORTED        00000020 /* BSW (byte swap) supported */
#define PDP8_SWP_SUPPORTED        00000040 /* SWP (AC <=> MQ) supported */
#define PDP8_EAE_HAS_MODE_B       00000100 /* EAE option has mode B instructions */
#define PDP8_EAE_UNSUPPORTED      00000200 /* EAE option is not supported */
#define PDP8_SCL_SUPPORTED        00000400 /* SCL instruction works in EAE */

typedef struct pdp8_model_flags_t {
    pdp8_model_t model;
    pdp8_rar_ral_t rarl;
    pdp8_rtr_rtl_t rtrl;
    unsigned flags;
} pdp8_model_flags_t;

typedef uint16_t uint12_t;
typedef struct pdp8_t pdp8_t;

/*
 * IOT device support
 */
#define PDP8_DEVICE_IDS 0100
typedef void (*device_handler_t)(uint12_t op, pdp8_t *pdp8);

/*
 * Why the machine halted
 */
typedef enum pdp8_halt_reason_t pdp8_halt_reason_t;
enum pdp8_halt_reason_t {
    PDP8_HALT_HLT_INSTRUCTION,
    PDP8_HALT_SWP_UNSUPPORTED,
    PDP8_HALT_IAC_ROTS_UNSUPPORTED,
    PDP8_HALT_CMA_ROTS_UNSUPPORTED,
    PDP8_HALT_CLA_NMI_UNSUPPORTED,
    PDP8_HALT_SCL_UNSUPPORTED,
};

/*
 * PDP8 emulator state 
 */
struct pdp8_t {
    uint12_t *core;
    int core_size;
    uint12_t ac;
    unsigned link : 1;
    unsigned run : 1;
    unsigned option_eae : 1;        /* Type 182 extended arithmetic element (i.e. mul and div) */
    unsigned eae_mode_b : 1;        /* Later EAE's: mode B instructions enabled */
    unsigned gt : 1;                /* EAE mode B GT register */
    uint12_t pc;
    uint12_t sr;        /* front panel switches */

    /* these registers are only used if option_eae is installed */
    uint12_t mq;
    uint12_t sc;

    pdp8_halt_reason_t halt_reason;
    pdp8_model_flags_t flags;

    device_handler_t device_handlers[PDP8_DEVICE_IDS];
};

extern pdp8_t *pdp8_create();
extern void pdp8_free(pdp8_t *pdp8);

/* default is PDP8_E */
extern int pdp8_set_model(pdp8_t *pdp8, pdp8_model_t model);

extern void pdp8_clear(pdp8_t *pdp8);
extern void pdp8_step(pdp8_t *pdp8);

/* utilities */
extern int pdp8_disassemble(uint16_t addr, uint12_t op, char *decoded, int decodedLen);
extern pdp8_reg_t pdp8_reg_from_string(char *s);

#endif
