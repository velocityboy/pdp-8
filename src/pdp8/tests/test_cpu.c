#include <stdio.h>
#include "tests.h"
#include "pdp8/emulator.h"

DECLARE_TEST(cpu_mem, "Basic memory addressing") {
    pdp8_t *pdp8 = pdp8_create();
    
    pdp8->core[01000] = PDP8_M_MAKE(PDP8_OP_AND, 0, 00020);
    pdp8->core[00020] = 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00700, "AND result correct");

    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_M_MAKE(PDP8_OP_AND, PDP8_M_PAGE, 00020);
    pdp8->core[01020] = 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;
    
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00700, "Same-page addressing correct");

    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_M_MAKE(PDP8_OP_AND, PDP8_M_INDIRECT, 00020);
    pdp8->core[00020] = 03000;
    pdp8->core[03000] = 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00700, "Indirect zero-page addressing correct");
    
    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_M_MAKE(PDP8_OP_AND, PDP8_M_PAGE|PDP8_M_INDIRECT, 00020);
    pdp8->core[01020] = 03000;
    pdp8->core[03000] = 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00700, "Indirect same-page addressing correct");

    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_M_MAKE(PDP8_OP_AND, PDP8_M_INDIRECT, 00010);
    pdp8->core[00010] = 03000;
    pdp8->core[03001] = 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00700, "Auto-indexing reads from correct address");
    ASSERT_V(pdp8->core[00010] == 03001, "Auto-indexing updates memory properly");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(cpu_AND, "AND instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_M_MAKE(PDP8_OP_AND, 0, 00020);
    pdp8->core[00020] = 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00700, "AND result correct");

    pdp8_free(pdp8);
}

