#include <stdio.h>
#include "tests.h"
#include "pdp8/devices.h"

DECLARE_TEST(intr_ION, "ION instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = 0;

    pdp8->pc = 01000;
    pdp8->intr_enable_mask = 0;

    pdp8_step(pdp8);
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts not yet enabled");

    pdp8_step(pdp8);
    ASSERT_V(pdp8_interrupts_enabled(pdp8), "Interrupts enabled");

    pdp8_free(pdp8);
}

DECLARE_TEST(intr_ION_CIF, "ION interacting with CIF") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    
    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_ION;
    pdp8->core[01002] = 0;
    pdp8->core[01003] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);

    pdp8->intr_enable_mask = PDP8_INTR_ION;    
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts disabled by CIF");

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts not re-enabled by just ION");

    pdp8_step(pdp8);
    ASSERT_V(pdp8_interrupts_enabled(pdp8), "Interrupts re-enabled by after JMP");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(intr_SKON, "SKON instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = 0;
    pdp8->core[01002] = PDP8_SKON;

    pdp8->pc = 01000;
    pdp8->intr_enable_mask = 0;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);

    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts are disabled");
    ASSERT_V(pdp8->pc == 01004, "skip performed");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_SKON;

    pdp8->pc = 01000;
    pdp8->intr_enable_mask = 0;

    pdp8_step(pdp8);

    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts are disabled");
    ASSERT_V(pdp8->pc == 01001, "no skip performed");

    pdp8_free(pdp8);
}

DECLARE_TEST(intr_IOF, "IOF instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = 0;
    pdp8->core[01002] = PDP8_IOF;

    pdp8->pc = 01000;
    pdp8->intr_enable_mask = 0;

    pdp8_step(pdp8);
    pdp8_step(pdp8);

    ASSERT_V(pdp8_interrupts_enabled(pdp8), "Interrupts are enabled");

    pdp8_step(pdp8);

    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts are disabled");

    pdp8_free(pdp8);
}

DECLARE_TEST(intr_SRQ, "SRQ instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_SRQ;

    pdp8->pc = 01000;
    pdp8->intr_mask = 0;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "no skip if no interrupts pending");

    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_SRQ;

    pdp8->pc = 01000;
    pdp8->intr_mask = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "skip if interrupts pending");

    pdp8_free(pdp8);
}

DECLARE_TEST(intr_GTF, "GTF instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_GTF;

    pdp8->pc = 01000;
    pdp8->link = 1;
    pdp8->intr_mask = 0;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 04000, "LINK sets bit in AC");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_GTF;

    pdp8->pc = 01000;
    pdp8->gt = 1;
    pdp8->intr_mask = 0;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 02000, "GT sets bit in AC");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_GTF;

    pdp8->pc = 01000;
    pdp8->intr_mask = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 01000, "pending interrupt sets bit in AC");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_GTF;

    pdp8->pc = 01000;
    pdp8->intr_mask = 0;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00200, "ION sets bit in AC");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_GTF;

    pdp8->pc = 01000;
    pdp8->intr_mask = 0;
    pdp8->sf = 045;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00045, "SF sets bits in AC");

    pdp8_free(pdp8);
}

DECLARE_TEST(intr_RTF, "RTF instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_RTF;

    pdp8->ac = BIT0;
    pdp8->pc = 01000;
    pdp8->link = 0;
    pdp8->gt = 0;
    pdp8->intr_mask = 0;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK set");
    ASSERT_V(pdp8->gt == 0, "GT not set");
    ASSERT_V(pdp8->ibr == 0, "IBR not set");
    ASSERT_V(pdp8->dfr == 0, "DFR not set");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_RTF;

    pdp8->ac = BIT1;
    pdp8->pc = 01000;
    pdp8->link = 0;
    pdp8->gt = 0;
    pdp8->intr_mask = 0;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK not set");
    ASSERT_V(pdp8->gt == 1, "GT set");
    ASSERT_V(pdp8->ibr == 0, "IBR not set");
    ASSERT_V(pdp8->dfr == 0, "DFR not set");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_RTF;

    pdp8->ac = 000040;
    pdp8->pc = 01000;
    pdp8->gt = 0;
    pdp8->link = 0;
    pdp8->intr_mask = 0;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK not set");
    ASSERT_V(pdp8->gt == 0, "GT set");
    ASSERT_V(pdp8->ibr == 040000, "IBR set");
    ASSERT_V(pdp8->dfr == 0, "DFR not set");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_RTF;

    pdp8->ac = 000005;
    pdp8->pc = 01000;
    pdp8->gt = 0;
    pdp8->link = 0;
    pdp8->intr_mask = 0;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK not set");
    ASSERT_V(pdp8->gt == 0, "GT set");
    ASSERT_V(pdp8->ibr == 0, "IBR set");
    ASSERT_V(pdp8->dfr == 050000, "DFR not set");

    /* NB RTF unconditionally executes an ION regardless of the ION bit in AC */

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_RTF;
    pdp8->core[01001] = 0;
    
    pdp8->ac = 000000;
    pdp8->pc = 01000;
    pdp8->gt = 0;
    pdp8->link = 0;
    pdp8->intr_mask = 0;
    pdp8->intr_enable_mask = 0;

    pdp8_step(pdp8);
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts not yet enabled");

    pdp8_step(pdp8);
    ASSERT_V(pdp8_interrupts_enabled(pdp8), "Interrupts enabled");

    pdp8_free(pdp8);
}

DECLARE_TEST(intr_SGT, "SGT instruction") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_SGT;

    pdp8->pc = 01000;
    pdp8->gt = 0;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01001, "Instruction not skipped if GT = 0");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_SGT;

    pdp8->pc = 01000;
    pdp8->gt = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 01002, "Instruction skipped if GT = 0");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(intr_interrupt, "interrupts handler") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_IOF;
    pdp8->core[01001] = PDP8_ION;
    pdp8->core[01002] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[00020] = PDP8_M_MAKE(PDP8_OP_TAD, 0, 00020);

    pdp8->core[00001] = PDP8_M_MAKE(PDP8_OP_TAD, 0, 00030);
    pdp8->core[00030] = 01234;
    
    pdp8->pc = 01000;

    pdp8_step(pdp8);    /* IOF */

    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "interrupts disabled");

    pdp8->intr_mask = 1;        /* request interrupt */

    pdp8_step(pdp8);    /* ION */

    ASSERT_V(pdp8->pc == 01002, "interrupt not yet serviced because of 1 instr delay");
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "interrupts disabled");

    pdp8_step(pdp8);    /* JMP */
    
    ASSERT_V(pdp8_interrupts_enabled(pdp8), "interrupts enabled");
    ASSERT_V(pdp8->pc == 00020, "interrupt vector not yet taken");
    
    pdp8_step(pdp8);    /* TAD */

    ASSERT_V(pdp8->ac == 01234, "ISR TAD instruction executed");
    ASSERT_V(pdp8->pc == 2, "Interrupt vectored to address 1");
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "interrupts disabled");
    ASSERT_V(pdp8->core[0] == 00020, "return address saved");
    
    pdp8_free(pdp8);
}