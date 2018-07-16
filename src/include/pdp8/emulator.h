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
#define PDP8_IOT0_FULL_INTR_SET   00000002 /* On early models, only ION and IOF are supported in IOT 0 */
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
typedef struct pdp8_device_t pdp8_device_t;

struct pdp8_device_t {
    pdp8_device_t *next;
    int (*install)(pdp8_device_t *dev, pdp8_t *pdp8);
    void (*reset)(pdp8_device_t *dev);
    void (*dispatch)(pdp8_device_t *dev, pdp8_t *pdp8, uint12_t opword);
    void (*free)(pdp8_device_t *dev);
};

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
    PDP8_HALT_CAF_HANG,
    PDP8_HALT_FRONT_PANEL,
    PDP8_HALT_DEVICE_REQUEST,
};

/* 
 * Returned error codes (always < 0)
 */
#define PDP8_ERR_INVALID_ARG (-1)
#define PDP8_ERR_CONFLICT (-2)              /* device conflicts w/ already installed device */
#define PDP8_ERR_MEMORY (-3)
#define PDP8_ERR_BUSY (-4)                  /* device is busy */
#define PDP8_ERR_FILEIO (-5)

/*
 * PDP8 emulator state 
 */
struct pdp8_t {
    uint12_t core[PDP8_MAX_CORE_WORDS];
    int core_size;
    uint12_t ac;
    unsigned link : 1;
    unsigned run : 1;
    unsigned option_eae : 1;        /* Type 182 extended arithmetic element (i.e. mul and div), or KE8-E */
    unsigned eae_mode_b : 1;        /* Later EAE's: mode B instructions enabled */
    unsigned gt : 1;                /* EAE mode B GT register */
    uint12_t pc;                    /* program counter */
    uint12_t sr;                    /* front panel switches */

    /* these registers are only used if option_eae is installed */
    uint12_t mq;
    uint12_t sc;

    /* KM8-E registers if there is more than 4K installed 
     * All field registers are stored shifted by 12 so they can just be or'ed with addresses
     * We preallocate max memory so it's always safe to read, and we guard writes based on 
     * core_size.
     */
    uint16_t ifr;           /* field for instruction fetches */
    uint16_t dfr;           /* field for data fetches */
    uint16_t ibr;           /* IF buffer - used during IF transitions */

    pdp8_halt_reason_t halt_reason;
    pdp8_model_flags_t flags;

    /*
     * intrerrupt management
     */
    int next_intr_bit;
    uint32_t intr_mask;
    uint32_t intr_enable_mask;
    int intr_enable_pend;           /* countdown to re-enable interrupts */
    uint12_t sf;                    /* save field - saves dfr and ifr on interrupt */

    pdp8_device_t *device_handlers[PDP8_DEVICE_IDS];
    pdp8_device_t *devices;

    /*
     * For devices to be able to schedule events after a certain number
     * of instructions
     */
    uint64_t instr_count;
    struct scheduler_t *scheduler;

    /*
     * Trace facility 
     */
    struct pdp8_trace_t *trace;
    char *tracefile;
};

/* flags for intr_enable_mask */
#define PDP8_INTR_ION            00001   /* interrupts are enabled */
#define PDP8_INTR_ION_PENDING    00002   /* ION instruction issued */
#define PDP8_INTR_IFR_PENDING    00004   /* IF register has changed (cleared upon JMP/JMS) */

static inline int pdp8_interrupts_enabled(pdp8_t *pdp8) {
    return pdp8->intr_enable_mask == PDP8_INTR_ION;
}

extern void pdp8_write_if_safe(pdp8_t *pdp8, uint16_t addr, uint12_t value);

extern pdp8_t *pdp8_create();
extern void pdp8_free(pdp8_t *pdp8);

/* default is PDP8_E */
extern int pdp8_set_model(pdp8_t *pdp8, pdp8_model_t model);

/* set the amount of memory. existing contents will be preserved (expect for the case where 
 * it shrinks), but dangerous if things are already running! 
 * memory is expressed in the number of 4k fields, 1 to 8.
 */
extern int pdp8_set_mex_fields(pdp8_t *pdp8, int fields);

/* install a device */
extern int pdp8_install_device(pdp8_t *pdp8, pdp8_device_t *dev);

/* run the machine */
extern void pdp8_clear(pdp8_t *pdp8);
extern void pdp8_step(pdp8_t *pdp8);


/* API for devices */
extern int pdp8_alloc_intr_bits(pdp8_t *pdp, int bits);
extern void pdp8_schedule(pdp8_t *pdp, int n, void (*callback)(void *), void *ctx);

/* for testing ONLY - fire everything in the scheduler */
extern void pdp8_drain_scheduler(pdp8_t *pdp8);

/* trace facility */
extern int pdp8_start_tracing(pdp8_t *pdp8, char *tracefile);
extern int pdp8_stop_tracing(pdp8_t *pdp8);
extern int pdp8_make_trace_listing(pdp8_t *pdp8, char *tracefile, char *listfile);

/* utilities */
extern int pdp8_disassemble(uint16_t addr, uint12_t *op, int eae_mode_b, char *decoded, int decoded_size);
extern pdp8_reg_t pdp8_reg_from_string(char *s);

#endif
