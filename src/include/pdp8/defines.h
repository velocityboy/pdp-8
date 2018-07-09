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

#define INC12(w) (((w) + 1) & MASK12)

/* memory sizes */
#define PDP8_FIELD_SIZE 4096
#define PDP8_MAX_FIELDS 8
#define PDP8_STD_CORE_WORDS PDP8_FIELD_SIZE
#define PDP8_MAX_CORE_WORDS (PDP8_MAX_FIELDS * PDP8_FIELD_SIZE)

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

#define PDP8_OPR_GRP2 07400
#define PDP8_OPR_GRP2_OSR BIT9
#define PDP8_OPR_GRP2_HLT BIT10

#define PDP8_OPR_GRP2_OR_SMA BIT5
#define PDP8_OPR_GRP2_OR_SZA BIT6
#define PDP8_OPR_GRP2_OR_SNL BIT7

#define PDP8_OPR_GRP2_AND BIT8
#define PDP8_OPR_GRP2_AND_SPA BIT5
#define PDP8_OPR_GRP2_AND_SNA BIT6
#define PDP8_OPR_GRP2_AND_SZL BIT7

#define PDP8_OPR_GRP3 07401
#define PDP8_OPR_GRP3_MQA BIT5
#define PDP8_OPR_GRP3_SCA BIT6
#define PDP8_OPR_GRP3_MQL BIT7
#define PDP8_OPR_GRP3_CODE(w) ((w) & 00016)

/* Group 3 with both MQA and MQL is a special case for SWP */
#define PDP8_OPR_GRP3_SWP 07521

typedef enum pdp8_grp3_code_t {
    PDP8_GRP3_CODE_NOP = 000,
    PDP8_GRP3_CODE_SCL = 002,
    PDP8_GRP3_CODE_MUY = 004,
    PDP8_GRP3_CODE_DVI = 006,
    PDP8_GRP3_CODE_NMI = 010,
    PDP8_GRP3_CODE_SHL = 012,
    PDP8_GRP3_CODE_ASR = 014,
    PDP8_GRP3_CODE_LSR = 016,
} pdp8_grp3_code_t;

#define PDP8_OPR_GRP3_CODE_B(w) ((w) & 00056)

typedef enum pdp8_grp3_code_b_t {
    PDP8_GRP3_CODE_B_NOP = 000,
    PDP8_GRP3_CODE_B_ACS = 002,
    PDP8_GRP3_CODE_B_MUY = 004,
    PDP8_GRP3_CODE_B_DVI = 006,
    PDP8_GRP3_CODE_B_NMI = 010,
    PDP8_GRP3_CODE_B_SHL = 012,
    PDP8_GRP3_CODE_B_ASR = 014,
    PDP8_GRP3_CODE_B_LSR = 016,
    PDP8_GRP3_CODE_B_SCA = 040,
    PDP8_GRP3_CODE_B_DAD = 042,
    PDP8_GRP3_CODE_B_DST = 044,
    PDP8_GRP3_CODE_B_SWBA = 046,
    PDP8_GRP3_CODE_B_DPSZ = 050,
    PDP8_GRP3_CODE_B_DPIC = 052,
    PDP8_GRP3_CODE_B_DCM = 054,
    PDP8_GRP3_CODE_B_SAM = 056,
} pdp8_grp3_code_b_t;

/* short cut definitions for mode switch */
#define PDP8_SWAB 07431
#define PDP8_SWBA 07447

#endif



