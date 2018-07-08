#include <stdio.h>
#include "tests.h"
#include "pdp8/emulator.h"

DECLARE_TEST(cpu_group2_CLA, "CLA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_CLA;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00000, "AC cleared");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SMA, "SMA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SMA;
    pdp8->ac = 00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC > 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SMA;
    pdp8->ac = 00000;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC == 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SMA;
    pdp8->ac = 07777;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on AC < 0");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SZA, "SZA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SZA;
    pdp8->ac = 00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC > 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SZA;
    pdp8->ac = 00000;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on AC == 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SZA;
    pdp8->ac = 07777;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC < 0");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SNL, "SNL instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SNL;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on link == 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SNL;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on link == 1");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SZA_SNL, "SZA SNL instruction") {
    pdp8_t *pdp8 = pdp8_create();

    /* with bit 8 == 0, skip is the OR of the specified conditions */

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SZA | PDP8_OPR_GRP2_OR_SNL;
    pdp8->ac = 00707;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC > 0 and LINK = 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SZA | PDP8_OPR_GRP2_OR_SNL;
    pdp8->ac = 00000;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on AC == 0 and LINK = 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_OR_SZA | PDP8_OPR_GRP2_OR_SNL;
    pdp8->ac = 1;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on AC == 1 and LINK == 1");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SPA, "SPA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SPA;
    pdp8->ac = 00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on AC > 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SPA;
    pdp8->ac = 00000;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on AC == 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SPA;
    pdp8->ac = 07777;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC < 0");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SNA, "SNA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SNA;
    pdp8->ac = 00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on AC != 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SNA;
    pdp8->ac = 00000;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC == 0");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SZL, "SZL instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SZL;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on LINK == 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SZL;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on LINK != 0");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SZL_SNA, "SZL SNA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    /* if the AND bit (bit 8) is set, then skip is only executed if ALL of the
     * conditions are met.
     */

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SZL | PDP8_OPR_GRP2_AND_SNA;
    pdp8->link = 1;
    pdp8->ac = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC == 1, LINK == 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SZL | PDP8_OPR_GRP2_AND_SNA;
    pdp8->link = 1;
    pdp8->ac = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC == 1, LINK == 1");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SZL | PDP8_OPR_GRP2_AND_SNA;
    pdp8->link = 0;
    pdp8->ac = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip on AC == 0, LINK == 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_AND_SZL | PDP8_OPR_GRP2_AND_SNA;
    pdp8->link = 0;
    pdp8->ac = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip on AC == 1, LINK == 0");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_SKP, "SKP instruction") {
    pdp8_t *pdp8 = pdp8_create();

    /* the degenerate case of setting the AND bit with no conditions is an
     * unconditional skip
     */

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip occurred");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_OSR, "OSR instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_OSR;
    pdp8->ac = 05050;
    pdp8->sr = 02222;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 07272, "AC or'ed with switch register");
    ASSERT_V(pdp8->sr == 02222, "SR not modified");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_HLT, "HLT instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_AND | PDP8_OPR_GRP2_HLT;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->run == 0, "HLT clears the RUN flip-flop");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_HLT_INSTRUCTION, "halt reason set to HLT");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group2_sequencing, "group 2 microinstruction sequencing") {
    pdp8_t *pdp8 = pdp8_create();

    /* sequencing should be skips => CLA => OSR => HLT */

    pdp8->core[01000] = 
        PDP8_OPR_GRP2 | 
        PDP8_OPR_CLA |
        PDP8_OPR_GRP2_AND | 
        PDP8_OPR_GRP2_AND_SPA |
        PDP8_OPR_GRP2_OSR | 
        PDP8_OPR_GRP2_HLT;
    pdp8->pc = 01000;
    pdp8->ac = 07777;
    pdp8->sr = 01234;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->run == 0, "HLT clears the RUN flip-flop");
    ASSERT_V(pdp8->ac == 01234, "AC loaded from SR");
    ASSERT_V(pdp8->pc == 01001, "SPA not executed");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_HLT_INSTRUCTION, "halt reason set to HLT");
    
    pdp8_free(pdp8);
}