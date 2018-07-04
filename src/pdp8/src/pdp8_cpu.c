#include "pdp8/defines.h"
#include "pdp8/emulator.h"

#include <stdlib.h>

static uint12_t effective_address(uint12_t op, pdp8_t *pdp8);
static void group_1(uint12_t op, pdp8_t *pdp8);
static void group_2_and(uint12_t op, pdp8_t *pdp8);
static void group_2_or(uint12_t op, pdp8_t *pdp8);
static void group3(uint12_t op, pdp8_t *pdp8);

/*
 * Create a new emulator instance.
 */
pdp8_t *pdp8_create() {
    pdp8_t *emu = calloc(1, sizeof(pdp8_t));
    if (emu == NULL) {
        return NULL;
    }

    emu->core_size = PDP8_STD_CORE_WORDS;
    emu->core = calloc(sizeof(uint12_t), emu->core_size);
    if (emu->core == NULL) {
        free(emu);
        return NULL;
    }

    return emu;
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
 * Execute one instruction.
 */
void pdp8_step(pdp8_t *pdp8) {
    if (!pdp8->run) {
        return;
    }

    uint12_t opword = pdp8->core[pdp8->pc];
    pdp8->pc = (pdp8->pc + 1) & MASK12;

    int op = PDP8_OP(opword);

    uint12_t ea;
    uint16_t sum;

    switch (op) {
        case PDP8_OP_AND:
            ea = effective_address(op, pdp8);
            pdp8->ac &= pdp8->core[ea];
            break;

        case PDP8_OP_TAD:
            ea = effective_address(op, pdp8);
            sum = pdp8->ac + pdp8->core[ea];
            if (sum & BIT_CARRY) {
                pdp8->link = ~pdp8->link;
            }
            break;

        case PDP8_OP_ISZ:
            ea = effective_address(op, pdp8);
            pdp8->core[ea] = (pdp8->core[ea] + 1) & MASK12;
            if (pdp8->core[ea] == 0) {
              pdp8->pc = (pdp8->pc + 1) & MASK12;
            }
            break;

        case PDP8_OP_DCA:
            ea = effective_address(op, pdp8);
            pdp8->core[ea] = pdp8->ac;
            pdp8->ac = 0;
            break;

        case PDP8_OP_JMS:
            ea = effective_address(op, pdp8);
            pdp8->core[ea] = pdp8->pc;
            pdp8->pc = (ea + 1) & MASK12;
            break;

        case PDP8_OP_JMP:
            pdp8->pc = effective_address(op, pdp8);
            break;

        case PDP8_OP_IOT: {
            void (*handler)(uint12_t, pdp8_t *) = pdp8->device_handlers[PDP8_IOT_DEVICE_ID(op)];
            if (handler == NULL) {
                handler(op, pdp8);
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
                group3(opword, pdp8);
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
        addr = pdp8->core[addr];
    }
  
    return addr;
}

/*
 * Execute an OPR group 1 instruction.
 */
static void group_1(uint12_t op, pdp8_t *pdp8) {
    /* CLA, CLL, CMA, CLL are all in event slot 1, but STL is CLL + CML, so
     * the clears must apply first.
     */
    if ((op & PDP8_OPR_CLA) != 0) { pdp8->ac = 0; }
    if ((op & PDP8_OPR_GRP1_CLL) != 0) { pdp8->link = 0; }
 
    if ((op & PDP8_OPR_GRP1_CMA) != 0) { pdp8->ac ^= 07777; }
    if ((op & PDP8_OPR_GRP1_CML) != 0) { pdp8->link = ~pdp8->link; }
 
    /* NOTE model specific - on the 8/I and later, IAC happens before the shifts; in 
     * earlier models, they happened at the same time.
     */
    if ((op & PDP8_OPR_GRP1_IAC) != 0) { pdp8->ac = (pdp8->ac + 1) & MASK12; }
 
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
    int skip = 0;
    
    if ((op & PDP8_OPR_GRP2_AND_SPA) != 0) {
        skip |= ((pdp8->ac & BIT0) != BIT0);
    }
    
    if ((op & PDP8_OPR_GRP2_AND_SNA) != 0) {
        skip |= pdp8->ac != 0;
    }
    
    if ((op & PDP8_OPR_GRP2_AND_SZL) != 0) {
        skip |= pdp8->link == 0;
    }
    
    if (skip) {
        pdp8->pc = (pdp8->pc + 1) & MASK12;
    }
    
    /* important: CLA happens *after* tests. */
    if ((op & PDP8_OPR_CLA) != 0) {
        pdp8->ac = 0;      
    }
    
    if ((op & PDP8_OPR_GRP2_HLT) != 0) {
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
        pdp8->run = 0;
    }
    
    if ((op & PDP8_OPR_GRP2_OSR) != 0) {
        pdp8->ac |= pdp8->sr;
    }
}

static void group3(uint12_t op, pdp8_t *pdp8) {

}
