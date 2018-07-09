#include "pdp8_eae.h"

static const int LSHIFT = 24;
static const int ACSHIFT = 12;
static const int AC0SHIFT = 23;
static const int AC1SHIFT = 22;

static void mode_a_codes(uint12_t op, pdp8_t *pdp8);
static void mode_b_codes(uint12_t op, pdp8_t *pdp8);

static void op_muy(pdp8_t *pdp8);
static void op_dvi(pdp8_t *pdp8);
static void op_nmi(pdp8_t *pdp8);
static void op_shl(pdp8_t *pdp8);
static void op_asr(pdp8_t *pdp8);
static void op_lsr(pdp8_t *pdp8);

static void op_b_acs(pdp8_t *pdp8);
static void op_b_muy(pdp8_t *pdp8);
static void op_b_dvi(pdp8_t *pdp8);
static void op_b_nmi(pdp8_t *pdp8);
static void op_b_dad(pdp8_t *pdp8);
static void op_b_dst(pdp8_t *pdp8);
static void op_b_dpsz(pdp8_t *pdp8);
static void op_b_dpic(pdp8_t *pdp8);
static void op_b_dcm(pdp8_t *pdp8);
static void op_b_sam(pdp8_t *pdp8);

static void muy(pdp8_t *pdp8, uint12_t term);
static void div(pdp8_t *pdp8, uint12_t divisor);    

void pdp8_group3(uint12_t op, pdp8_t *pdp8) {
    /* first, take care of potential mode changes */
    if ((pdp8->flags.flags & PDP8_EAE_HAS_MODE_B) != 0) {
        switch (op) {
            case PDP8_SWAB:
                pdp8->eae_mode_b = 1;
                return;
            case PDP8_SWBA:
                pdp8->eae_mode_b = 0;
                return;
        }
    }

    /* on early machines, CLA+NMI with AC != 0 would hang */
    if ((pdp8->flags.flags & PDP8_CLA_NMI_HANGS) != 0 &&
        pdp8->ac != 0 &&
        PDP8_OPR_GRP3_CODE(op) == PDP8_GRP3_CODE_NMI) {
        pdp8->run = 0;
        pdp8->halt_reason = PDP8_HALT_CLA_NMI_UNSUPPORTED;
        return;
    }

    int eae = pdp8->option_eae && ((pdp8->flags.flags & PDP8_EAE_UNSUPPORTED) == 0);

    /* per the docs, in mode A the GT FF (which only exists if mode B exists) is always cleared. */
    if ((pdp8->flags.flags & PDP8_EAE_HAS_MODE_B) != 0 && !pdp8->eae_mode_b) {
        pdp8->gt = 0;
    }

    if ((op & PDP8_OPR_CLA) != 0) {
        pdp8->ac = 0;
    }

    /* if both MQA and MQL are set, then MQ and AC are exchanged */
    const uint12_t SWP = PDP8_OPR_GRP3_MQA | PDP8_OPR_GRP3_MQL;
    if ((op & SWP) == SWP) {
        if ((pdp8->flags.flags & PDP8_SWP_SUPPORTED) == 0) {
            pdp8->run = 0;
            pdp8->halt_reason = PDP8_HALT_SWP_UNSUPPORTED;
        }
        uint12_t ac = pdp8->ac;
        pdp8->ac = pdp8->mq;
        pdp8->mq = ac;
    } else {
        if ((op & PDP8_OPR_GRP3_MQA) != 0) {
            pdp8->ac |= pdp8->mq;
        }

        if ((op & PDP8_OPR_GRP3_MQL) != 0) {
            pdp8->mq = pdp8->ac;
            pdp8->ac = 0;
        }
    }

    /* in mode B, the SCA bit is not supported and becomes part of the instruction code */
    if (!pdp8->eae_mode_b) {
        if (eae && (op & PDP8_OPR_GRP3_SCA) != 0) {
            pdp8->ac |= (pdp8->sc & 00037);
        }
    }

    if (eae) {
        if (pdp8->eae_mode_b) {
            mode_b_codes(op, pdp8);
        } else {
            mode_a_codes(op, pdp8);
        }
    }
}

static void mode_a_codes(uint12_t op, pdp8_t *pdp8) {
    switch (PDP8_OPR_GRP3_CODE(op)) {
        case PDP8_GRP3_CODE_NOP:
            break;

        case PDP8_GRP3_CODE_SCL:
            if ((pdp8->flags.flags & PDP8_SCL_SUPPORTED) == 0) {
                pdp8->run = 0;
                pdp8->halt_reason = PDP8_HALT_SCL_UNSUPPORTED;
                return;
            }
            pdp8->sc = (~pdp8->core[pdp8->pc]) & 00037;
            pdp8->pc = (pdp8->pc + 1) & 07777;
            break;

        case PDP8_GRP3_CODE_MUY:
            op_muy(pdp8);
            break;

        case PDP8_GRP3_CODE_DVI:
            op_dvi(pdp8);
            break;

        case PDP8_GRP3_CODE_NMI:
            op_nmi(pdp8);
            break;

        case PDP8_GRP3_CODE_SHL:
            op_shl(pdp8);
            break;

        case PDP8_GRP3_CODE_ASR:
            op_asr(pdp8);
            break;

        case PDP8_GRP3_CODE_LSR:
            op_lsr(pdp8);
            break;
    }    
}

/* multiply and divide. these are unsigned, not 2's complement, operations */

static void op_muy(pdp8_t *pdp8) {
    uint12_t term = pdp8->core[pdp8->pc];
    pdp8->pc = (pdp8->pc + 1) & MASK12;
    muy(pdp8, term);
}

static void op_dvi(pdp8_t *pdp8) {
    uint12_t divisor = pdp8->core[pdp8->pc];
    pdp8->pc = (pdp8->pc + 1) & MASK12;
    div(pdp8, divisor);
}

static void op_nmi(pdp8_t *pdp8) {
    uint32_t temp = (pdp8->link << 24) | (pdp8->ac << 12) | pdp8->mq;

    int sc = 0;

    if (temp) {
        while (1) {
            if ((temp & 077777777) == 060000000) {
                break;
            }
            int ac0 = (temp >> AC0SHIFT) & 01;
            int ac1 = (temp >> AC1SHIFT) & 01;
            if (ac0 != ac1) {
                break;
            }
            temp <<= 1;
            sc++;
        }
    }

    /* in mode B, if the result is 4000 0000 (link is ignored), then
     * AC is cleared. 
     */
    if (pdp8->eae_mode_b && (temp & 077777777) == 040000000) {
        temp &= ~040000000;
    }

    pdp8->sc = sc;
    pdp8->link = (temp >> LSHIFT) & 01;
    pdp8->ac = (temp >> ACSHIFT) & MASK12;
    pdp8->mq = temp & MASK12;
}

static void op_shl(pdp8_t *pdp8) {
    int shifts = (pdp8->core[pdp8->pc] & 037);
    if (!pdp8->eae_mode_b) {
        shifts++;
    }

    pdp8->pc = (pdp8->pc + 1) & MASK12;
    uint32_t temp = (pdp8->link << 24) | (pdp8->ac << 12) | pdp8->mq;
    
    temp <<= shifts;

    pdp8->sc = 0;
    pdp8->link = (temp >> LSHIFT) & 01;
    pdp8->ac = (temp >> ACSHIFT) & MASK12;
    pdp8->mq = temp & MASK12;    
}

static void op_asr(pdp8_t *pdp8) {
    int shifts = (pdp8->core[pdp8->pc] & 037);
    if (!pdp8->eae_mode_b) {
        shifts++;
    }

    pdp8->pc = (pdp8->pc + 1) & MASK12;
    int32_t temp = 0;
    if (pdp8->ac & BIT0) {
        temp = -1 & ~077777777;
    }
    temp |= (pdp8->ac << 12) | pdp8->mq;

    if (pdp8->eae_mode_b && shifts != 0) {
        pdp8->gt = (temp & (1 << (shifts - 1))) ? 1 : 0;
    }
    
    temp >>= shifts;

    pdp8->sc = 0;
    pdp8->link = (temp >> LSHIFT) & 01;
    pdp8->ac = (temp >> ACSHIFT) & MASK12;
    pdp8->mq = temp & MASK12;    
}

static void op_lsr(pdp8_t *pdp8) {
    int shifts = (pdp8->core[pdp8->pc] & 037);
    if (!pdp8->eae_mode_b) {
        shifts++;
    }

    pdp8->pc = (pdp8->pc + 1) & MASK12;
    uint32_t temp = (pdp8->ac << 12) | pdp8->mq;

    if (pdp8->eae_mode_b && shifts != 0) {
        pdp8->gt = (temp & (1 << (shifts - 1))) ? 1 : 0;
    }

    temp >>= shifts;

    pdp8->sc = 0;
    pdp8->link = 0;
    pdp8->ac = (temp >> ACSHIFT) & MASK12;
    pdp8->mq = temp & MASK12;    
}

/*-----------------------------------------------------------------------------
 * Mode B implementations
 */

static void mode_b_codes(uint12_t op, pdp8_t *pdp8) {
    switch (PDP8_OPR_GRP3_CODE_B(op)) {
        case PDP8_GRP3_CODE_B_NOP:
            break;

        case PDP8_GRP3_CODE_B_ACS:
            op_b_acs(pdp8);
            break;

        case PDP8_GRP3_CODE_B_MUY:
            op_b_muy(pdp8);
            break;

        case PDP8_GRP3_CODE_B_DVI:
            op_b_dvi(pdp8);
            break;

        case PDP8_GRP3_CODE_B_NMI:
            /* not a mistake, code is shared here */
            op_nmi(pdp8);
            break;

        case PDP8_GRP3_CODE_B_SHL:
            /* not a mistake, code is shared here */
            op_shl(pdp8);
            break;

        case PDP8_GRP3_CODE_B_ASR:
            /* not a mistake, code is shared here */
            op_asr(pdp8);
            break;

        case PDP8_GRP3_CODE_B_LSR:
            /* not a mistake, code is shared here */
            op_lsr(pdp8);
            break;

        case PDP8_GRP3_CODE_B_SCA:
            /* this happens to be the mode A SCA bit + mode A NOP */
            pdp8->ac |= (pdp8->sc & 00037);
            break;

        case PDP8_GRP3_CODE_B_DAD:
            op_b_dad(pdp8);
            break;

        case PDP8_GRP3_CODE_B_DST:
            op_b_dst(pdp8);
            break;

        case PDP8_GRP3_CODE_B_DPSZ:
            op_b_dpsz(pdp8);
            break;

        case PDP8_GRP3_CODE_B_DPIC:
            op_b_dpic(pdp8);
            break;            

        case PDP8_GRP3_CODE_B_DCM:
            op_b_dcm(pdp8);
            break; 
            
        case PDP8_GRP3_CODE_B_SAM:
            op_b_sam(pdp8);
            break;
    } 
}

static void op_b_acs(pdp8_t *pdp8) {
    pdp8->sc = pdp8->ac & 00037;
    pdp8->ac = 0;
}

static void op_b_muy(pdp8_t *pdp8) {    
    uint12_t addr = pdp8->core[pdp8->pc];
    if ((pdp8->pc & 07770) == 00010) {
        addr = (addr + 1) & MASK12;
        pdp8->core[pdp8->pc] = addr;
    }

    pdp8->pc = (pdp8->pc + 1) & MASK12;
    uint12_t term = pdp8_read_data_word(pdp8, addr);
    muy(pdp8, term);
}

static void op_b_dvi(pdp8_t *pdp8) {
    uint12_t addr = pdp8->core[pdp8->pc];
    if ((pdp8->pc & 07770) == 00010) {
        addr = (addr + 1) & MASK12;
        pdp8->core[pdp8->pc] = addr;
    }
    pdp8->pc = (pdp8->pc + 1) & MASK12;
    uint12_t divisor = pdp8_read_data_word(pdp8, addr);
    div(pdp8, divisor);
}

static void op_b_dad(pdp8_t *pdp8) {
    uint12_t addr = pdp8->core[pdp8->pc];
    if ((pdp8->pc & 07770) == 00010) {
        addr = (addr + 1) & MASK12;
        pdp8->core[pdp8->pc] = addr;
    }
    pdp8->pc = (pdp8->pc + 1) & MASK12;

    uint32_t x = (pdp8->ac << ACSHIFT) | pdp8->mq;
    uint32_t y = pdp8_read_data_word(pdp8, addr);
    addr = (addr + 1) & MASK12;
    y |= pdp8_read_data_word(pdp8, addr) << 12;

    uint32_t sum = x + y;

    pdp8->link = ((sum >> LSHIFT) != 0) ? 1 : 0;
    pdp8->ac = (sum >> ACSHIFT) & MASK12;
    pdp8->mq = sum & MASK12;
}

static void op_b_dst(pdp8_t *pdp8) {
    uint12_t addr = pdp8->core[pdp8->pc];
    if ((pdp8->pc & 07770) == 00010) {
        addr = (addr + 1) & MASK12;
        pdp8->core[pdp8->pc] = addr;
    }
    pdp8->pc = (pdp8->pc + 1) & MASK12;

    pdp8_write_data_word(pdp8, addr, pdp8->mq);
    addr = (addr + 1) & MASK12;
    pdp8_write_data_word(pdp8, addr, pdp8->ac);
}

static void op_b_dpsz(pdp8_t *pdp8) {
    if (pdp8->ac == 0 && pdp8->mq == 0) {
        pdp8->pc = (pdp8->pc + 1) & MASK12;
    }
}

static void op_b_dpic(pdp8_t *pdp8) {
    uint32_t acmq = (pdp8->ac << ACSHIFT) | pdp8->mq;
    acmq++;

    pdp8->link = ((acmq >> LSHIFT) != 0) ? 1 : 0;
    pdp8->ac = (acmq >> ACSHIFT) & MASK12;
    pdp8->mq = acmq & MASK12;
}

static void op_b_dcm(pdp8_t *pdp8) {
    uint32_t acmq = (pdp8->ac << ACSHIFT) | pdp8->mq;
    acmq = ((~acmq) & 077777777) + 1;

    pdp8->link = ((acmq >> LSHIFT) != 0) ? 1 : 0;
    pdp8->ac = (acmq >> ACSHIFT) & MASK12;
    pdp8->mq = acmq & MASK12;
}

static void op_b_sam(pdp8_t *pdp8) {
    pdp8->link = pdp8->ac > pdp8->mq ? 1 : 0;

    /* sign extend AC and MQ to native word size */
    int sac = pdp8->ac | ((pdp8->ac & BIT0) ? ~MASK12 : 0);
    int smq = pdp8->mq | ((pdp8->mq & BIT0) ? ~MASK12 : 0);

    pdp8->gt = sac <= smq ? 1 : 0;
}

/*-----------------------------------------------------------------------------
 * Common code for routines which only differ in addressing between modes.
 */

static void muy(pdp8_t *pdp8, uint12_t term) {
    /* NB per the 1972 handbook, AC is added after the multiply. */
    uint32_t product = term * pdp8->mq + pdp8->ac;
    pdp8->link = 0;
    pdp8->ac = product >> 12;
    pdp8->mq = product & MASK12;
    pdp8->sc = 12;
}

static void div(pdp8_t *pdp8, uint12_t divisor) {
    if (divisor == 0) {
        pdp8->link = 1;     /* indicates overflow */
        return;
    }

    uint32_t dividend = (pdp8->ac << 12) | pdp8->mq;
    uint32_t quot = dividend / divisor;
    uint32_t rem  = dividend % divisor;

    if (quot > 07777) {
        /* AC and MQ are documented as changing here but not how */
        pdp8->link = 1;     /* indicates overflow */
        return;
    }

    pdp8->mq = quot;
    pdp8->ac = rem;
    pdp8->link = 0;
    pdp8->sc = 13;
}

