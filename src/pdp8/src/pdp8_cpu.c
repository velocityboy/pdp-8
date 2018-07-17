#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pdp8/defines.h"
#include "pdp8/emulator.h"

#include "pdp8_eae.h"
#include "pdp8_trace.h"
#include "scheduler.h"

static uint16_t effective_address(uint12_t op, uint12_t page, pdp8_t *pdp8);
static void group_1(uint12_t op, pdp8_t *pdp8);
static void group_2_and(uint12_t op, pdp8_t *pdp8);
static void group_2_or(uint12_t op, pdp8_t *pdp8);
static void cpu_iots(uint12_t op, pdp8_t *pdp8);

static pdp8_model_flags_t models[] = {
    {
        PDP8,
        PDP8_RARL_UNDEFINED,
        PDP8_RTRL_UNDEFINED,
        PDP8_CMA_ROTS_SUPPORTED |
        PDP8_CLA_NMI_HANGS,
    },
    {
        PDP8_S,
        PDP8_RARL_UNDEFINED,
        PDP8_RTRL_UNDEFINED,
        PDP8_CLA_NMI_HANGS |
        PDP8_SCL_SUPPORTED,
    },
    {
        PDP8_I,
        PDP8_RARL_AND,
        PDP8_RTRL_AND,
        PDP8_IAC_ROTS_SUPPORTED |
        PDP8_CMA_ROTS_SUPPORTED |
        PDP8_CLA_NMI_HANGS |
        PDP8_SWP_SUPPORTED |
        PDP8_SCL_SUPPORTED,
    },
    {
        PDP8_L,
        PDP8_RARL_AND,
        PDP8_RTRL_AND,
        PDP8_IAC_ROTS_SUPPORTED |
        PDP8_CMA_ROTS_SUPPORTED |
        PDP8_CLA_NMI_HANGS |
        PDP8_EAE_UNSUPPORTED,
    },
    {
        PDP8_E,
        PDP8_RARL_AND_INSTR,
        PDP8_RTRL_PAGE_INSTR,
        PDP8_IOT0_FULL_INTR_SET |
        PDP8_IAC_ROTS_SUPPORTED |
        PDP8_CMA_ROTS_SUPPORTED |
        PDP8_SWP_SUPPORTED |
        PDP8_BSW_SUPPORTED |
        PDP8_SCL_SUPPORTED |
        PDP8_EAE_HAS_MODE_B,
    },
    {
        PDP8_A,
        PDP8_RARL_AND_INSTR,
        PDP8_RTRL_NEXT_ADDR,
        PDP8_IOT0_FULL_INTR_SET |
        PDP8_IAC_ROTS_SUPPORTED |
        PDP8_CMA_ROTS_SUPPORTED |
        PDP8_SWP_SUPPORTED |
        PDP8_BSW_SUPPORTED |
        PDP8_SCL_SUPPORTED |
        PDP8_EAE_HAS_MODE_B,
    },    
};

static int model_count = sizeof(models) / sizeof(models[0]);

/*
 * Create a new emulator instance.
 */
pdp8_t *pdp8_create() {
    pdp8_t *pdp8 = calloc(1, sizeof(pdp8_t));
    if (pdp8 == NULL) {
        return NULL;
    }

    pdp8->core_size = PDP8_STD_CORE_WORDS;
    pdp8->run = 1;
    pdp8->scheduler = scheduler_create();

    pdp8_set_model(pdp8, PDP8_E);

    return pdp8;
}

/*
 * Free an emulator.
 */
void pdp8_free(pdp8_t *pdp8) {
    if (pdp8) {
        while (pdp8->devices) {
            pdp8_device_t *next = pdp8->devices->next;
            pdp8->devices->free(pdp8->devices);
            pdp8->devices = next;
        }
        scheduler_free(pdp8->scheduler);
        free(pdp8);
    }
}

/*
 * Set the model
 */
int pdp8_set_model(pdp8_t *pdp8, pdp8_model_t model) {
    int found = 0;
    for (int i = 0; i < model_count; i++) {
        if (models[i].model == model) {
            pdp8->flags = models[i];
            found = 1;
            break;
        }
    }

    return found ? 0 : PDP8_ERR_INVALID_ARG;
}

/* set available memory 
 */
int pdp8_set_mex_fields(pdp8_t *pdp8, int fields) {
    if (fields < 1 || fields > PDP8_MAX_FIELDS) {
        return PDP8_ERR_INVALID_ARG;
    }

    pdp8->core_size = PDP8_FIELD_SIZE * fields;
    return 0;
}

int pdp8_install_device(pdp8_t *pdp8, pdp8_device_t *dev) {
    int ret = dev->install(dev, pdp8);
    if (ret < 0) {
        return ret;
    }

    dev->next = pdp8->devices;
    pdp8->devices = dev;

    return 0;
}

/*
 * Return the entire emulator (including core) to a blank slate.
 * Does not change what model the machine is.
 */
extern void pdp8_clear(pdp8_t *pdp8) {
    memset(pdp8->core, 0, PDP8_MAX_CORE_WORDS * sizeof(uint12_t));
    pdp8->ac = 0;
    pdp8->link = 0;
    pdp8->run = 1;
    pdp8->option_eae = 0;
    pdp8->eae_mode_b = 0;
    pdp8->gt = 0;
    pdp8->pc = 0;
    pdp8->sr = 0;
    pdp8->mq = 0;
    pdp8->sc = 0;
    pdp8->ifr = 0;
    pdp8->dfr = 0;
    pdp8->ibr = 0;
    pdp8->intr_mask = 0;
    /* at startup, interrupts are off until enabled *and* a JMP/JMS has executed */
    pdp8->intr_enable_mask = PDP8_INTR_IFR_PENDING;
    pdp8->intr_enable_pend = 0;
    pdp8->sf = 0;

    for (pdp8_device_t *dev = pdp8->devices; dev; dev = dev->next) {
        dev->reset(dev);
    }

    scheduler_clear(pdp8->scheduler);
}

int pdp8_start_tracing(pdp8_t *pdp8, char *tracefile) {
    if (pdp8->trace) {
        return PDP8_ERR_BUSY;
    }

    pdp8->trace = pdp8_trace_create(pdp8);
    pdp8->tracefile = strdup(tracefile);

    return 0;
}

int pdp8_stop_tracing(pdp8_t *pdp8) {
    if (!pdp8->trace) {
        return PDP8_ERR_INVALID_ARG;
    }

    if (pdp8_trace_save(pdp8->trace, pdp8->tracefile) < 0) {
        return PDP8_ERR_FILEIO;
    }

    pdp8_trace_free(pdp8->trace);
    free(pdp8->tracefile);

    pdp8->trace = NULL;
    pdp8->tracefile = NULL;

    return 0;
}

int pdp8_make_trace_listing(pdp8_t *pdp8, char *tracefile, char *listfile) {
    if (pdp8->trace) {
        return PDP8_ERR_BUSY;
    }

    pdp8_trace_t *trace = pdp8_trace_load(tracefile);
    if (trace == NULL) {
        return PDP8_ERR_FILEIO;
    }

    if (pdp8_trace_save_listing(trace, listfile) < 0) {
        pdp8_trace_free(trace);
        return PDP8_ERR_FILEIO;        
    }

    pdp8_trace_free(trace);
    return 0;
}

/* all writes should be gated through this */
void pdp8_write_if_safe(pdp8_t *pdp8, uint16_t addr, uint12_t value) {
    if (pdp8->trace) {
        pdp8_trace_memory_write(pdp8->trace, addr, value);
    }

    if (addr < pdp8->core_size) {
        pdp8->core[addr] = value;
    }
}

/*
 * Execute one instruction.
 */
void pdp8_step(pdp8_t *pdp8) {
    if (!pdp8->run) {
        return;
    }

    /* handle deferred events */
    pdp8->instr_count++;

    /* getting the time returns MAXINT if empty */
    scheduler_t *sch = pdp8->scheduler;
    while (scheduler_next_event_time(sch) <= pdp8->instr_count) {
        scheduler_callback_t callback;
        int ret = scheduler_extract_min(sch, &callback);
        assert(ret >= 0);
        callback.event(callback.ctx);
    }

    /* first, handle pending interrupts */
    if (pdp8_interrupts_enabled(pdp8) && pdp8->intr_mask) {
        pdp8->intr_enable_mask &= ~PDP8_INTR_ION;
        pdp8->sf = 
            ((pdp8->dfr >> 12) & 007) |
            ((pdp8->ifr >>  9) & 070);
        pdp8->dfr = 0;
        pdp8->ifr = 0;
        pdp8_write_if_safe(pdp8, 0, pdp8->pc);
        pdp8->pc = 1;
        if (pdp8->trace) {
            pdp8_trace_interrupt(pdp8->trace);
        }
    }

    /* then, update any pending interrupt state */
    pdp8->intr_enable_mask &= ~PDP8_INTR_ION_PENDING;

    if (pdp8->trace) {
        pdp8_trace_begin_instruction(pdp8->trace);
    }

    uint12_t opword = pdp8->core[pdp8->ifr | pdp8->pc];
    uint12_t page = pdp8->pc & 07600;
    pdp8->pc = INC12(pdp8->pc);

    int oper = PDP8_OP(opword);

    uint16_t ea;
    uint12_t temp;
    uint16_t sum;

    switch (oper) {
        case PDP8_OP_AND:
            ea = effective_address(opword, page, pdp8);
            pdp8->ac &= pdp8->core[ea];
            break;

        case PDP8_OP_TAD: 
            ea = effective_address(opword, page, pdp8);
            sum = (pdp8->link << 12) + pdp8->ac + pdp8->core[ea];
            pdp8->ac = sum & 07777;
            pdp8->link = (sum >> 12) & 01;
            break;

        case PDP8_OP_ISZ:
            ea = effective_address(opword, page, pdp8);
            temp = INC12(pdp8->core[ea]);
            pdp8_write_if_safe(pdp8, ea, temp);

            if (temp == 0) {
              pdp8->pc = INC12(pdp8->pc);
            }
            break;

        case PDP8_OP_DCA:
            ea = effective_address(opword, page, pdp8);
            pdp8_write_if_safe(pdp8, ea, pdp8->ac);
            pdp8->ac = 0;
            break;

        case PDP8_OP_JMS:
            pdp8->ifr = pdp8->ibr;
            ea = effective_address(opword, page, pdp8) & MASK12;
            pdp8_write_if_safe(pdp8, pdp8->ifr | ea, pdp8->pc);
            pdp8->pc = INC12(ea);
            pdp8->intr_enable_mask &= ~PDP8_INTR_IFR_PENDING;
            break;

        case PDP8_OP_JMP:
            pdp8->ifr = pdp8->ibr;
            pdp8->pc = effective_address(opword, page, pdp8) & MASK12;
            pdp8->intr_enable_mask &= ~PDP8_INTR_IFR_PENDING;
            break;

        case PDP8_OP_IOT: {
            if (PDP8_IOT_DEVICE_ID(opword) == 0) {
                cpu_iots(opword, pdp8);
            } else {
                pdp8_device_t *dev = pdp8->device_handlers[PDP8_IOT_DEVICE_ID(opword)];
                if (dev) {
                    dev->dispatch(dev, pdp8, opword);
                }
            }
            break;
        }

        case PDP8_OP_OPR:
            if (PDP8_OPR_GROUP1(opword)) {
                group_1(opword, pdp8);
            } else if (PDP8_OPR_GROUP2_OR(opword)) {
                group_2_or(opword, pdp8);
            } else if (PDP8_OPR_GROUP2_AND(opword)) {
                group_2_and(opword, pdp8);
            } else if (PDP8_OPR_GROUP3(opword)) {
                pdp8_group3(opword, pdp8);
            }
            break;
    }

    if (pdp8->trace) {
        pdp8_trace_end_instruction(pdp8->trace);
    }
}

/*
 * Alloc interrupt state bits 
 */
int pdp8_alloc_intr_bits(pdp8_t *pdp8, int bits) {
    int maxbits = sizeof(pdp8->intr_mask) * 8;
    int bit0 = pdp8->next_intr_bit;
    if (bit0 + bits > maxbits) {
        return PDP8_ERR_MEMORY;
    }
    pdp8->next_intr_bit += bits;
    return bit0;
}

/* 
 * Schedule a callback in 'n' instructions
 */
void pdp8_schedule(pdp8_t *pdp8, int n, void (*callback)(void *), void *ctx) {
    scheduler_insert(pdp8->scheduler, pdp8->instr_count + n, callback, ctx);
}

/*
 * Remove all matching scheduled events
 */
void pdp8_unschedule(pdp8_t *pdp8, void (*callback)(void *), void *ctx) {
    scheduler_delete(pdp8->scheduler, callback, ctx);
}

/* 
 * For testing, it's useful to be able to skip directly to queued
 * events.
 */
void pdp8_drain_scheduler(pdp8_t *pdp8) {
    while (!scheduler_empty(pdp8->scheduler)) {
        scheduler_callback_t callback;
        if (scheduler_extract_min(pdp8->scheduler, &callback) < 0) {
            break;
        }

        callback.event(callback.ctx);
    }
}

/*
 * Compute the effective address for a memory operation, taking 
 * indirection and PC relative addressing into account.
 */
static uint16_t effective_address(uint12_t op, uint12_t page, pdp8_t *pdp8) {
    /* low 7 bits come from op encoding */
    uint16_t addr = PDP8_M_OFFS(op);
  
    /* bit 4, if set, means in same page as PC */
    if (PDP8_M_ZERO(op)) {
        addr |= page;
    }

    addr |= pdp8->ifr;
  
    /* if addressing is direct, operand is taken using IF */
    /* if indirect, indirect address is from IF and operand from DF */

    /* bit 3, if set, means indirect */
    if (PDP8_M_IND(op)) {
        /* auto-indexing */
        uint12_t ea = pdp8->core[addr];

        if ((addr & 07770) == 00010) {
            /* per docs, each *field* contains 8 autoindex locations, so it is
             * correct to have IF or'ed into addr here.
             */
            ea = INC12(ea);
            pdp8_write_if_safe(pdp8, addr, ea);
        }

        addr = ea | pdp8->dfr;        
    } 
  
    return addr;
}

/*
 * Execute an OPR group 1 instruction.
 */
static void group_1(uint12_t op, pdp8_t *pdp8) {
    /* On some models, some bit combos are unsupported. */
    if ((op & (PDP8_OPR_GRP1_RAL | PDP8_OPR_GRP1_RAR)) != 0) {
        unsigned flags = pdp8->flags.flags;
        if (((op & PDP8_OPR_GRP1_CMA) != 0) && ((flags & PDP8_CMA_ROTS_SUPPORTED) == 0)) {
            pdp8->run = 0;
            pdp8->halt_reason = PDP8_HALT_CMA_ROTS_UNSUPPORTED;
            return;
        }

        if (((op & PDP8_OPR_GRP1_IAC) != 0) && ((flags & PDP8_IAC_ROTS_SUPPORTED) == 0)) {
            pdp8->run = 0;
            pdp8->halt_reason = PDP8_HALT_IAC_ROTS_UNSUPPORTED;
            return;
        }
    }

    /* CLA, CLL, CMA, CLL are all in event slot 1, but STL is CLL + CML, so
     * the clears must apply first.
     */
    if ((op & PDP8_OPR_CLA) != 0) { 
        pdp8->ac = 0; 
    }
    
    if ((op & PDP8_OPR_GRP1_CLL) != 0) {
         pdp8->link = 0; 
    }
 
    if ((op & PDP8_OPR_GRP1_CMA) != 0) { 
        pdp8->ac ^= 07777; 
    }

    if ((op & PDP8_OPR_GRP1_CML) != 0) { 
        pdp8->link = ~pdp8->link; 
    }
 
    /* NOTE model specific - on the 8/I and later, IAC happens before the shifts; in 
     * earlier models, they happened at the same time.
     */
    if ((op & PDP8_OPR_GRP1_IAC) != 0) { 
        pdp8->ac = (pdp8->ac + 1) & MASK12; 
    }
 
    uint12_t shift = op & (PDP8_OPR_GRP1_RAL | PDP8_OPR_GRP1_RAR);
 
    int by = ((op & PDP8_OPR_GRP1_RTWO) != 0) ? 2 : 1;
    if (shift == 0) {
        /* model specific - on 8/E and later, the double-shift bit by itself
         * swaps the low and high 6 bits of AC
         */       
        if ((pdp8->flags.flags & PDP8_BSW_SUPPORTED) != 0 && (op & PDP8_OPR_GRP1_RTWO) != 0) {
            pdp8->ac =
                ((pdp8->ac & 077600) >> 6) | ((pdp8->ac & 000177) << 6);
        }
    } else if (shift == PDP8_OPR_GRP1_RAL) {
        while (by--) {
            pdp8->ac <<= 1;
            pdp8->ac |= pdp8->link;
            pdp8->link = (pdp8->ac >> 12) & 01;
            pdp8->ac = (pdp8->ac) & MASK12;
        }
    } else if (shift == PDP8_OPR_GRP1_RAR) {
        while (by--) {
            uint16_t l = pdp8->ac & 01;        
            pdp8->ac >>= 1;
            pdp8->ac |= (pdp8->link << 11);
            pdp8->link = l;
        }
    }    
}

/*
 * Group 2 instructions, AND version.
 */
static void group_2_and(uint12_t op, pdp8_t *pdp8) {
    int skip = 1;
    
    if ((op & PDP8_OPR_GRP2_AND_SPA) != 0) {
        skip &= ((pdp8->ac & BIT0) != BIT0);
    }
    
    if ((op & PDP8_OPR_GRP2_AND_SNA) != 0) {
        skip &= pdp8->ac != 0;
    }
    
    if ((op & PDP8_OPR_GRP2_AND_SZL) != 0) {
        skip &= pdp8->link == 0;
    }
    
    if (skip) {
        pdp8->pc = (pdp8->pc + 1) & MASK12;
    }
    
    /* important: CLA happens *after* tests. */
    if ((op & PDP8_OPR_CLA) != 0) {
        pdp8->ac = 0;      
    }
    
    if ((op & PDP8_OPR_GRP2_HLT) != 0) {
        pdp8->halt_reason = PDP8_HALT_HLT_INSTRUCTION;
        pdp8->run = 0;
    }
    
    if ((op & PDP8_OPR_GRP2_OSR) != 0) {
        pdp8->ac |= pdp8->sr;
    }    
}

static void group_2_or(uint12_t op, pdp8_t *pdp8) {
    int skip = 0;
    
    if ((op & PDP8_OPR_GRP2_OR_SMA) != 0) {
        skip |= ((pdp8->ac & BIT0) == BIT0);
    }
    
    if ((op & PDP8_OPR_GRP2_OR_SZA) != 0) {
        skip |= pdp8->ac == 0;
    }
    
    if ((op & PDP8_OPR_GRP2_OR_SNL) != 0) {
        skip |= pdp8->link != 0;
    }
    
    if (skip) {
        pdp8->pc = (pdp8->pc + 1) & MASK12;
    }
    
    /* important: CLA happens *after* tests. */
    if ((op & PDP8_OPR_CLA) != 0) {
        pdp8->ac = 0;      
    }
    
    if ((op & PDP8_OPR_GRP2_HLT) != 0) {
        pdp8->halt_reason = PDP8_HALT_HLT_INSTRUCTION;
        pdp8->run = 0;
    }
    
    if ((op & PDP8_OPR_GRP2_OSR) != 0) {
        pdp8->ac |= pdp8->sr;
    }
}

static void cpu_iots(uint12_t op, pdp8_t *pdp8) {
    if (op != PDP8_ION && op != PDP8_IOF && (pdp8->flags.flags & PDP8_IOT0_FULL_INTR_SET) == 0) {
        pdp8->run = 0;
        pdp8->halt_reason = PDP8_HALT_CAF_HANG;
        return;
    }

    switch (op) {
        case PDP8_SKON:
            if (pdp8_interrupts_enabled(pdp8)) {
                pdp8->pc = INC12(pdp8->pc);
                pdp8->intr_enable_mask &= ~PDP8_INTR_ION;
            }
            break;

        case PDP8_ION:
            if (!pdp8_interrupts_enabled(pdp8)) {
                /* execution will continue to the end of this instruction, and the end
                 * of the next, and THEN interrupts will be enabled.
                 */
                pdp8->intr_enable_mask |= PDP8_INTR_ION | PDP8_INTR_ION_PENDING;
            }
            break;
        
        case PDP8_IOF:
            pdp8->intr_enable_mask &= ~PDP8_INTR_ION;
            break;

        case PDP8_SRQ:
            if (pdp8->intr_mask) {
                pdp8->pc = INC12(pdp8->pc);                
            }
            break;

        case PDP8_GTF:
            pdp8->ac = 
                (pdp8->link ? BIT0 : 0)  |
                (pdp8->gt ? BIT1 : 0) |
                (pdp8->intr_mask ? BIT2 : 0) |
                ((pdp8->intr_enable_mask & PDP8_INTR_ION) ? BIT4 : 0) |
                (pdp8->sf & 00177);
            break;

        case PDP8_RTF:
            pdp8->link = (pdp8->ac & BIT0) ? 1 : 0;
            pdp8->gt = (pdp8->ac & BIT1) ? 1 : 0;
            pdp8->ibr = (pdp8->ac << 9) & 070000;
            pdp8->dfr = (pdp8->ac << 12) & 070000;
            pdp8->intr_enable_mask |= PDP8_INTR_ION_PENDING | PDP8_INTR_ION;
            break;

        case PDP8_SGT:
            /* NB this is a NOP on machines w/o the KE8-E installed. However, the GT
             * flag can only ever be set by that option, so no need to check.
             */
            if (pdp8->gt) {
                pdp8->pc = INC12(pdp8->pc);                
            }
            break;

        case PDP8_CAF: {
            for (pdp8_device_t *dev = pdp8->devices; dev; dev = dev->next) {
                dev->reset(dev);
            } 
            pdp8->link = 0;
            pdp8->ac = 0;
            break;
        }                        
    }
}


