#include <stdio.h>
#include <string.h>

#include "tests.h"
#include "pdp8/devices.h"

typedef struct callback_mock_t {
    int rdr_ready;
    uint8_t punched;
} callback_mock_t;

static void pun_free(void *ctx) {

}

static void pun_rdr_ready(void *ctx) {
    ((callback_mock_t *)ctx)->rdr_ready++;
}

static void pun_punch(void *ctx, uint8_t ch) {
    ((callback_mock_t *)ctx)->punched = ch;
}

static int setup(callback_mock_t *mocks, pdp8_t **pdp8, pdp8_punch_t **pun) {
    *pdp8 = pdp8_create();

    *pun = NULL;

    memset(mocks, 0, sizeof(*mocks));

    pdp8_punch_callbacks_t callbacks;
    callbacks.ctx = mocks;
    callbacks.free = &pun_free;
    callbacks.rdr_ready = &pun_rdr_ready;
    callbacks.punch = &pun_punch;

    int ret = pdp8_install_punch(*pdp8, &callbacks, pun);
    if (ret < 0) {
        pdp8_free(*pdp8);
    }

    return ret;
}

DECLARE_TEST(pun_RSF_RRB_RFC, "RSF, RRB and RFC instructions") {
    pdp8_t *pdp8 = NULL;
    pdp8_punch_t *pun = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &pun);
    ASSERT_V(ret >= 0, "created and installed punch");

    if (ret < 0) {
        return;
    }

    pdp8_punch_rdr_byte(pun, '@');

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0001);
    /* we should interrupt at the start of this instruction */
    pdp8->core[01002] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);

    pdp8->core[1] = RSF;
    pdp8->core[2] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[3] = RRB;
    pdp8->core[4] = RFC;
    pdp8->core[5] = RSF;
    
    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* ION */
    pdp8_step(pdp8);        /* first JMP */
    pdp8_step(pdp8);        /* takes interrupt, executes RSF */

    ASSERT_V(callback_mocks.rdr_ready == 0, "rdr_ready not yet called");
    pdp8_step(pdp8);        /* RRB (HLT should be skipped) */
    ASSERT_V(pdp8->ac == '@', "Reader data received"); 

    /* RRB alone does not clear the reader flag */
    ASSERT_V(callback_mocks.rdr_ready == 0, "rdr_ready not yet called");
    pdp8_drain_scheduler(pdp8);
    ASSERT_V(callback_mocks.rdr_ready == 0, "rdr_ready not yet called");
    
    pdp8_step(pdp8);        /* RFC */

    /* reader ready is scheduled so host can't sent bytes too fast */
    ASSERT_V(callback_mocks.rdr_ready == 0, "rdr_ready not yet called");
    pdp8_drain_scheduler(pdp8);
    ASSERT_V(callback_mocks.rdr_ready == 1, "rdr_ready called");
        
    ASSERT_V(pdp8->pc == 5, "Interrupt routine executed");

    pdp8_step(pdp8);        /* RSF (should not skip) */
    ASSERT_V(pdp8->pc == 6, "RSF did not skip after flag cleared");

    pdp8_free(pdp8);
}

DECLARE_TEST(pun_RSF_RRBRFC, "RSF and RRB_RFC instructions") {
    pdp8_t *pdp8 = NULL;
    pdp8_punch_t *pun = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &pun);
    ASSERT_V(ret >= 0, "created and installed punch");

    if (ret < 0) {
        return;
    }

    pdp8_punch_rdr_byte(pun, '@');

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0001);
    /* we should interrupt at the start of this instruction */
    pdp8->core[01002] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);

    pdp8->core[1] = RSF;
    pdp8->core[2] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[3] = RRB_RFC;
    pdp8->core[4] = RSF;
    
    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* ION */
    pdp8_step(pdp8);        /* first JMP */
    pdp8_step(pdp8);        /* takes interrupt, executes RSF */

    ASSERT_V(callback_mocks.rdr_ready == 0, "rdr_ready not yet called");
    pdp8_step(pdp8);        /* RRB_RFC (HLT should be skipped) */
    ASSERT_V(pdp8->ac == '@', "Reader data received"); 

    /* reader ready is scheduled so host can't sent bytes too fast */
    ASSERT_V(callback_mocks.rdr_ready == 0, "rdr_ready not yet called");
    pdp8_drain_scheduler(pdp8);
    ASSERT_V(callback_mocks.rdr_ready == 1, "rdr_ready called");
        
    ASSERT_V(pdp8->pc == 4, "Interrupt routine executed");

    pdp8_step(pdp8);        /* RSF (should not skip) */
    ASSERT_V(pdp8->pc == 5, "RSF did not skip after flag cleared");

    pdp8_free(pdp8);
}

DECLARE_TEST(pun_reader_interrupts, "RPE and PCE instructions on reader") {
    pdp8_t *pdp8 = NULL;
    pdp8_punch_t *pun = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &pun);
    ASSERT_V(ret >= 0, "created and installed punch");

    if (ret < 0) {
        return;
    }

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);
    pdp8->core[01002] = PCE;
    pdp8->core[01003] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0004);
    pdp8->core[01004] = RPE;
    pdp8->core[01005] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0006);
    
    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* ION */
    pdp8_step(pdp8);        /* first JMP */
    pdp8_step(pdp8);        /* PCE */
    pdp8_punch_rdr_byte(pun, '@');
    pdp8_step(pdp8);        /* JMP */
    
    /* we should NOT have interrupted since PCE was executed */    
    ASSERT_V(pdp8->pc == 01004, "disabled interrupt did not fire"); 

    pdp8_step(pdp8);        /* RPE */
    pdp8_step(pdp8);        /* JMP */

    ASSERT_V(pdp8->core[0] == 01005, "interrupt return stored");
    ASSERT_V(pdp8->pc == 2, "interrupt taken");

    pdp8_free(pdp8);
}

DECLARE_TEST(pun_PLS, "PLS instructions") {
    pdp8_t *pdp8 = NULL;
    pdp8_punch_t *pun = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &pun);
    ASSERT_V(ret >= 0, "created and installed punch");

    if (ret < 0) {
        return;
    }

    pdp8->core[01000] = PDP8_IOF;
    pdp8->core[01001] = PLS;    
    pdp8->core[01002] = PSF;
    pdp8->core[01003] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);
    pdp8->core[01004] = PLS;
    pdp8->core[01005] = PSF;
    pdp8->core[01006] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0005);
    
    pdp8->pc = 01000;
    pdp8->ac = 0357;
    callback_mocks.punched = 0;
    
    pdp8_step(pdp8);        /* IOF */
    pdp8_step(pdp8);        /* PLS */
    pdp8_step(pdp8);        /* PSF */

    ASSERT_V(pdp8->pc == 01003, "punch flag not immediately set");

    for (int i = 0; i < 100; i++) {
        pdp8_step(pdp8);
        if (pdp8->pc == 01004) {
            break;
        }
    }

    ASSERT_V(pdp8->pc == 01004, "punch flag eventually set");

    pdp8->ac = 0327;
    callback_mocks.punched = 0;
    pdp8_step(pdp8);        /* PLS */
    ASSERT_V(callback_mocks.punched == 0327, "character punched");

    pdp8_step(pdp8);        /* PSF */
    ASSERT_V(pdp8->pc == 01006, "punch flag cleared by PLS");
        
    pdp8_free(pdp8);
}

DECLARE_TEST(pun_punch_interrupts, "RPE and PCE instructions on punch") {
    pdp8_t *pdp8 = NULL;
    pdp8_punch_t *pun = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &pun);
    ASSERT_V(ret >= 0, "created and installed punch");

    if (ret < 0) {
        return;
    }

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);
    pdp8->core[01002] = PCE;
    pdp8->core[01003] = PLS;
    pdp8->core[01004] = PSF;    
    pdp8->core[01005] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0004);    
    pdp8->core[01006] = RPE;
    pdp8->core[01007] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0006);    
    
    pdp8->pc = 01000;

    for (int i = 0; i < 100; i++) {
        pdp8_step(pdp8);
        if (pdp8->pc == 01006) {
            break;
        }
    }

    ASSERT_V(pdp8->pc == 01006, "punch flag set but interrupt did not fire");

    pdp8_step(pdp8);        /* RPE */
    pdp8_step(pdp8);        /* ??? */

    ASSERT_V(pdp8->pc == 00002, "interrupt fired after RPE");
    
    pdp8_free(pdp8);
}
