#ifndef _PDP8_DEFINES_H_
#define _PDP8_DEFINES_H_

/*
 * Definitions for the PDP8 architecture
 */

/* PDP8 bits start with 0 at the high bit */
#define BIT_CARRY 010000
#define BIT0 04000
#define BIT1 02000
#define BIT2 01000
#define BIT3 00400
#define BIT4 00200
#define BIT5 00100
#define BIT6 00040
#define BIT7 00020
#define BIT8 00010
#define BIT9 00004
#define BIT10 00002
#define BIT11 00001

#define MASK12 07777

/* memory sizes */
#define PDP8_STD_CORE_WORDS 4096
#define PDP8_MEX_CORE_WORDS 32768

/* registers */
typedef enum pdp8_reg_t {
    REG_INVALID = -1,
    REG_AC,
    REG_PC,
    REG_LINK,
    REG_RUN,
    REG_SR,
    REG_SC,
    REG_MQ,
} pdp8_reg_t;

/* instructions */
typedef enum pdp8_op_t {
    PDP8_OP_AND = 00,
    PDP8_OP_TAD = 01,
    PDP8_OP_ISZ = 02,
    PDP8_OP_DCA = 03,
    PDP8_OP_JMS = 04,
    PDP8_OP_JMP = 05,
    PDP8_OP_IOT = 06,
    PDP8_OP_OPR = 07,
} pdp8_op_t;

#define PDP8_OP(w) (((w) >> 9) & 07)

/* memory instruction layout */
#define PDP8_M_IND(w) ((w) & BIT3)
#define PDP8_M_ZERO(w) ((w) & BIT4)
#define PDP8_M_OFFS(w) ((w) & 00177)

#define PDP8_M_INDIRECT BIT3
#define PDP8_M_PAGE BIT4

#define PDP8_M_MAKE(op, opts, offset) (((op) << 9) | (opts) | (offset))

/* IOT layout */
#define PDP8_IOT_DEVICE_ID(w) (((w) >> 3) & 077)
#define PDP8_IOT_FUNCTION(w) ((w) & 07)

/* OPR instruction decoding */
#define PDP8_OPR_GROUP1(w) (((w) & 07400) == 07000)
#define PDP8_OPR_GROUP2_OR(w) (((w) & 07411) == 07400)
#define PDP8_OPR_GROUP2_AND(w) (((w) & 07411) == 07410)
#define PDP8_OPR_GROUP3(w) (((w) & 07401) == 07401)

/* CLA is consistent across all groups */
#define PDP8_OPR_CLA BIT4

#define PDP8_OPR_GRP1 07000
#define PDP8_OPR_GRP1_CLL BIT5
#define PDP8_OPR_GRP1_CMA BIT6
#define PDP8_OPR_GRP1_CML BIT7
#define PDP8_OPR_GRP1_RAR BIT8
#define PDP8_OPR_GRP1_RAL BIT9
#define PDP8_OPR_GRP1_BSW BIT10
#define PDP8_OPR_GRP1_RTWO BIT10
#define PDP8_OPR_GRP1_IAC BIT11

#define PDP8_OPR_GRP2_OSR BIT9
#define PDP8_OPR_GRP2_HLT BIT10

#define PDP8_OPR_GRP2_OR_SMA BIT5
#define PDP8_OPR_GRP2_OR_SZA BIT6
#define PDP8_OPR_GRP2_OR_SNL BIT7

#define PDP8_OPR_GRP2_AND_SPA BIT5
#define PDP8_OPR_GRP2_AND_SNA BIT6
#define PDP8_OPR_GRP2_AND_SZL BIT7

#define PDP8_OPR_GRP3_MQA BIT5
#define PDP8_OPR_GRP3_SCA BIT6
#define PDP8_OPR_GRP3_MQL BIT7
#define PDP8_OPR_GRP3_CODE(w) (((w) & 00016) >> 1)

typedef enum pdp8_grp3_code {
    PDP8_GRP3_CODE_NOP,
    PDP8_GRP3_CODE_SCL,
    PDP8_GRP3_CODE_MUY,
    PDP8_GRP3_CODE_DVI,
    PDP8_GRP3_CODE_NMI,
    PDP8_GRP3_CODE_SHL,
    PDP8_GRP3_CODE_ASR,
    PDP8_GRP3_CODE_LSR,
} pdp8_grp3_code;

#endif



