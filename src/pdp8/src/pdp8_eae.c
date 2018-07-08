#include "pdp8_eae.h"

static const int LSHIFT = 24;
static const int ACSHIFT = 12;
static const int AC0SHIFT = 23;
static const int AC1SHIFT = 22;

static void op_muy(pdp8_t *pdp8);
static void op_dvi(pdp8_t *pdp8);
static void op_nmi(pdp8_t *pdp8);
static void op_shl(pdp8_t *pdp8);
static void op_asr(pdp8_t *pdp8);
static void op_lsr(pdp8_t *pdp8);

void pdp8_group3(uint12_t op, pdp8_t *pdp8) {
    int eae = pdp8->option_eae && ((pdp8->flags.flags & PDP8_EAE_UNSUPPORTED) == 0);

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

    if (eae && (op & PDP8_OPR_GRP3_SCA) != 0) {
        pdp8->ac |= pdp8->sc;
    }

    if (!eae) {
        return;
    }

    switch (PDP8_OPR_GRP3_CODE(op)) {
        case PDP8_GRP3_CODE_NOP:
            break;

        case PDP8_GRP3_CODE_SCL:
            /* TODO on 8/I and forward, load SC from memory */
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
    /* NB per the 1972 handbook, AC is added after the multiply. */
    uint32_t product = term * pdp8->mq + pdp8->ac;
    pdp8->link = 0;
    pdp8->ac = product >> 12;
    pdp8->mq = product & MASK12;
    pdp8->sc = 12;
}

static void op_dvi(pdp8_t *pdp8) {
    uint12_t divisor = pdp8->core[pdp8->pc];
    pdp8->pc = (pdp8->pc + 1) & MASK12;
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

    pdp8->sc = sc;
    pdp8->link = (temp >> LSHIFT) & 01;
    pdp8->ac = (temp >> ACSHIFT) & MASK12;
    pdp8->mq = temp & MASK12;
}

static void op_shl(pdp8_t *pdp8) {
    int shifts = (pdp8->core[pdp8->pc] & 037) + 1;
    pdp8->pc = (pdp8->pc + 1) & MASK12;
    uint32_t temp = (pdp8->link << 24) | (pdp8->ac << 12) | pdp8->mq;
    
    temp <<= shifts;

    pdp8->sc = 0;
    pdp8->link = (temp >> LSHIFT) & 01;
    pdp8->ac = (temp >> ACSHIFT) & MASK12;
    pdp8->mq = temp & MASK12;    
}

static void op_asr(pdp8_t *pdp8) {
    int shifts = (pdp8->core[pdp8->pc] & 037) + 1;
    pdp8->pc = (pdp8->pc + 1) & MASK12;
    int32_t temp = 0;
    if (pdp8->ac & BIT0) {
        temp = -1 & ~077777777;
    }
    temp |= (pdp8->ac << 12) | pdp8->mq;
    
    temp >>= shifts;

    pdp8->sc = 0;
    pdp8->link = (temp >> LSHIFT) & 01;
    pdp8->ac = (temp >> ACSHIFT) & MASK12;
    pdp8->mq = temp & MASK12;    
}

static void op_lsr(pdp8_t *pdp8) {
    int shifts = (pdp8->core[pdp8->pc] & 037) + 1;
    pdp8->pc = (pdp8->pc + 1) & MASK12;
    uint32_t temp = (pdp8->ac << 12) | pdp8->mq;

    temp >>= shifts;

    pdp8->sc = 0;
    pdp8->link = 0;
    pdp8->ac = (temp >> ACSHIFT) & MASK12;
    pdp8->mq = temp & MASK12;    
}
