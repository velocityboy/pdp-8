#include <stdio.h>
#include "tests.h"
#include "pdp8/emulator.h"

DECLARE_TEST(cpu_group1_CLA, "CLA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_CLA;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00000, "AC cleared");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_CLL, "CLL instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_CLL;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 00000, "LINK cleared");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_CMA, "CMA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_CMA;
    pdp8->ac = 00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 07070, "AC complemented");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_CML, "CML instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_CML;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK complemented");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_CLL_CML, "CLL CML instruction") {
    pdp8_t *pdp8 = pdp8_create();

    /* sequencing should guarantee that this combo sets LINK */
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_CLL | PDP8_OPR_GRP1_CML;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK set");

    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_CLL | PDP8_OPR_GRP1_CML;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK set");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_CLA_CMA, "CLA CMA instruction") {
    pdp8_t *pdp8 = pdp8_create();

    /* sequencing should guarantee that this combo sets AC to all ones */
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_CLA | PDP8_OPR_GRP1_CMA;
    pdp8->ac = 01234;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 07777, "AC set to all ones");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_RAR, "RAR instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_RAR;
    pdp8->ac = 01235;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00516, "AC properly rotated");
    ASSERT_V(pdp8->link == 1, "LINK properly rotated");

    pdp8_clear(pdp8);
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_RAR;
    pdp8->ac = 01234;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 04516, "AC properly rotated");
    ASSERT_V(pdp8->link == 0, "LINK properly rotated");

    pdp8_clear(pdp8);
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_RAR | PDP8_OPR_GRP1_RTWO;
    pdp8->ac = 01234;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 02247, "AC properly rotated");
    ASSERT_V(pdp8->link == 0, "LINK properly rotated");

    pdp8_clear(pdp8);
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_RAR | PDP8_OPR_GRP1_RTWO;
    pdp8->ac = 01236;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00247, "AC properly rotated");
    ASSERT_V(pdp8->link == 1, "LINK properly rotated");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_RAL, "RAL instruction") {
    pdp8_t *pdp8 = pdp8_create();
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_RAL;
    pdp8->ac = 04235;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 0472, "AC properly rotated");
    ASSERT_V(pdp8->link == 1, "LINK properly rotated");

    pdp8_clear(pdp8);
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_RAL;
    pdp8->ac = 00235;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00473, "AC properly rotated");
    ASSERT_V(pdp8->link == 0, "LINK properly rotated");

    pdp8_clear(pdp8);
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_RAL | PDP8_OPR_GRP1_RTWO;
    pdp8->ac = 00235;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 01166, "AC properly rotated");
    ASSERT_V(pdp8->link == 0, "LINK properly rotated");

    pdp8_clear(pdp8);
    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_RAL | PDP8_OPR_GRP1_RTWO;
    pdp8->ac = 02235;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 01164, "AC properly rotated");
    ASSERT_V(pdp8->link == 1, "LINK properly rotated");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_IAC, "IAC instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_IAC;
    pdp8->ac = 00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00710, "AC incremented");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_IAC_RTR, "IAC RTL instruction") {
    pdp8_t *pdp8 = pdp8_create();

    /* proper sequencing is CLA -> IAC -> RTL which should give AC=4 */

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_CLA | PDP8_OPR_GRP1_IAC | PDP8_OPR_GRP1_RAL | PDP8_OPR_GRP1_RTWO;
    pdp8->ac = 00707;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00004, "AC loaded with 4");
    ASSERT_V(pdp8->link == 0, "LINK cleared");

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_CLA | PDP8_OPR_GRP1_IAC | PDP8_OPR_GRP1_RAL | PDP8_OPR_GRP1_RTWO;
    pdp8->ac = 00707;
    pdp8->link = 1;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00006, "AC loaded with 6");
    ASSERT_V(pdp8->link == 0, "LINK cleared");

    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_group1_unsupported, "model-specific unsupported microinstruction combinations") {
    pdp8_t *pdp8 = pdp8_create();
    pdp8_set_model(pdp8, PDP8_S);    
    
    /* ROT + CMA unsupported on 8/S */    
    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_CMA | PDP8_OPR_GRP1_RAL;
    pdp8->ac = 00707;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->run == 0, "CPU halted on CMA+ROT on PDP8_S");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_CMA_ROTS_UNSUPPORTED, "Halt reason set on CMA+ROT on PDP8_S");

    /* ROT + IAC unsupported on straight 8 and 8/S */    
    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8);

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_IAC | PDP8_OPR_GRP1_RAL;
    pdp8->ac = 00707;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->run == 0, "CPU halted on IAC+ROT on PDP8");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_IAC_ROTS_UNSUPPORTED, "Halt reason set on IAC+ROT on PDP8");
 
    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_S);

    pdp8->core[01000] = PDP8_OPR_GRP1 | PDP8_OPR_GRP1_IAC | PDP8_OPR_GRP1_RAL;
    pdp8->ac = 00707;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->run == 0, "CPU halted on IAC+ROT on PDP8_S");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_IAC_ROTS_UNSUPPORTED, "Halt reason set on IAC+ROT on PDP8_S");

    pdp8_free(pdp8);    
}