#include <stdlib.h>
#include <string.h>

#include "pdp8/defines.h"
#include "pdp8/emulator.h"

#include "pdp8_eae.h"

static uint12_t effective_address(uint12_t op, pdp8_t *pdp8);
static void group_1(uint12_t op, pdp8_t *pdp8);
static void group_2_and(uint12_t op, pdp8_t *pdp8);
static void group_2_or(uint12_t op, pdp8_t *pdp8);


static pdp8_model_flags_t models[] = {
    {
        PDP8,
        PDP8_RARL_UNDEFINED,
        PDP8_RTRL_UNDEFINED,
        PDP8_IAC_ROTS_UNSUPPORTED |
        PDP8_IOT0_IS_IAC |
        PDP8_CLA_NMI_HANGS,
    },
    {
        PDP8_S,
        PDP8_RARL_UNDEFINED,
        PDP8_RTRL_UNDEFINED,
        PDP8_IAC_ROTS_UNSUPPORTED |
        PDP8_CMA_ROTS_UNSUPPORTED |
        PDP8_CLA_NMI_HANGS,
    },
    {
        PDP8_I,
        PDP8_RARL_AND,
        PDP8_RTRL_AND,
        PDP8_CLA_NMI_HANGS |
        PDP8_SWP_SUPPORTED,
    },
    {
        PDP8_L,
        PDP8_RARL_AND,
        PDP8_RTRL_AND,
        PDP8_CLA_NMI_HANGS |
        PDP8_EAE_UNSUPPORTED,
    },
    {
        PDP8_E,
        PDP8_RARL_AND_INSTR,
        PDP8_RTRL_PAGE_INSTR,
        PDP8_SWP_SUPPORTED |
        PDP8_BSW_SUPPORTED |
        PDP8_EAE_HAS_MODE_B,
    },
    {
        PDP8_A,
        PDP8_RARL_AND_INSTR,
        PDP8_RTRL_NEXT_ADDR,
        PDP8_SWP_SUPPORTED |
        PDP8_BSW_SUPPORTED |
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
    pdp8->core = calloc(sizeof(uint12_t), pdp8->core_size);
    if (pdp8->core == NULL) {
        free(pdp8);
        return NULL;
    }
    pdp8->run = 1;

    pdp8_set_model(pdp8, PDP8_E);

    return pdp8;
}

/*
 * Free an emulator.
 */
void pdp8_free(pdp8_t *pdp8) {
    if (pdp8) {
        free(pdp8->core);
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

    return found ? 0 : -1;
}


/*
 * Return the entire emulator (including core) to a blank slate.
 * Does not change what model the machine is.
 */
extern void pdp8_clear(pdp8_t *pdp8) {
    uint12_t *core = pdp8->core;
    int core_size = pdp8->core_size;
    pdp8_model_flags_t flags = pdp8->flags;

    memset(pdp8, 0, sizeof(pdp8_t));
    memset(core, 0, core_size * sizeof(uint12_t));

    pdp8->core = core;
    pdp8->core_size = core_size;
    pdp8->run = 1;

    pdp8->flags = flags;
}

/*
 * Execute one instruction.
 */
void pdp8_step(pdp8_t *pdp8) {
    if (!pdp8->run) {
        return;
    }

    uint12_t opword = pdp8->core[pdp8->pc];
    pdp8->pc = (pdp8->pc + 1) & MASK12;

    int oper = PDP8_OP(opword);

    uint12_t ea;
    uint16_t sum;

    switch (oper) {
        case PDP8_OP_AND:
            ea = effective_address(opword, pdp8);
            pdp8->ac &= pdp8->core[ea];
            break;

        case PDP8_OP_TAD: 
            ea = effective_address(opword, pdp8);
            sum = (pdp8->link << 12) + pdp8->ac + pdp8->core[ea];
            pdp8->ac = sum & 07777;
            pdp8->link = (sum >> 12) & 01;
            break;

        case PDP8_OP_ISZ:
            ea = effective_address(opword, pdp8);
            pdp8->core[ea] = (pdp8->core[ea] + 1) & MASK12;
            if (pdp8->core[ea] == 0) {
              pdp8->pc = (pdp8->pc + 1) & MASK12;
            }
            break;

        case PDP8_OP_DCA:
            ea = effective_address(opword, pdp8);
            pdp8->core[ea] = pdp8->ac;
            pdp8->ac = 0;
            break;

        case PDP8_OP_JMS:
            ea = effective_address(opword, pdp8);
            pdp8->core[ea] = pdp8->pc;
            pdp8->pc = (ea + 1) & MASK12;
            break;

        case PDP8_OP_JMP:
            pdp8->pc = effective_address(opword, pdp8);
            break;

        case PDP8_OP_IOT: {
            void (*handler)(uint12_t, pdp8_t *) = pdp8->device_handlers[PDP8_IOT_DEVICE_ID(opword)];
            if (handler == NULL) {
                handler(opword, pdp8);
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
}

/*
 * Compute the effective address for a memory operation, taking 
 * indirection and PC relative addressing into account.
 */
static uint12_t effective_address(uint12_t op, pdp8_t *pdp8) {
    /* low 7 bits come from op encoding */
    uint12_t addr = PDP8_M_OFFS(op);
  
    /* bit 4, if set, means in same page as PC */
    if (PDP8_M_ZERO(op)) {
        uint12_t page = pdp8->pc & 07600;
        addr |= page;
    }
  
    /* bit 3, if set, means indirect */
    if (PDP8_M_IND(op)) {
        /* auto-indexing */
        if (addr >= 00010 && addr <= 00017) {
            pdp8->core[addr] = (pdp8->core[addr] + 1) & MASK12;
        }

        addr = pdp8->core[addr];
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
        if (((op & PDP8_OPR_GRP1_CMA) != 0) && ((flags & PDP8_CMA_ROTS_UNSUPPORTED) != 0)) {
            pdp8->run = 0;
            pdp8->halt_reason = PDP8_HALT_CMA_ROTS_UNSUPPORTED;
            return;
        }

        if (((op & PDP8_OPR_GRP1_IAC) != 0) && ((flags & PDP8_IAC_ROTS_UNSUPPORTED) != 0)) {
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
        if ((op & PDP8_OPR_GRP1_RTWO) != 0) {
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

