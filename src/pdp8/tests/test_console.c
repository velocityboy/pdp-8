#include <stdio.h>
#include <string.h>

#include "tests.h"
#include "pdp8/devices.h"

typedef struct callback_mock_t {
    int kbd_ready;
    char printed;
} callback_mock_t;

static void tty_free(void *ctx) {

}

static void tty_kbd_ready(void *ctx) {
    ((callback_mock_t *)ctx)->kbd_ready++;
}

static void tty_print(void *ctx, char ch) {
    ((callback_mock_t *)ctx)->printed = ch;
}

static void tty_rdr_ready(void *ctx) {

}

static void tty_punch(void *ctx, char ch) {

}

static int setup(callback_mock_t *mocks, pdp8_t **pdp8, pdp8_console_t **con) {
    *pdp8 = pdp8_create();

    *con = NULL;

    memset(mocks, 0, sizeof(*mocks));

    pdp8_console_callbacks_t callbacks;
    callbacks.ctx = mocks;
    callbacks.free = &tty_free;
    callbacks.kbd_ready = &tty_kbd_ready;
    callbacks.print = &tty_print;
    callbacks.rdr_ready = &tty_rdr_ready;
    callbacks.punch = &tty_punch;

    int ret = pdp8_install_console(*pdp8, &callbacks, con);
    if (ret < 0) {
        pdp8_free(*pdp8);
    }

    return ret;
}

DECLARE_TEST(con_KSF_KRB, "KSF and KRB instructions") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }

    pdp8_console_kbd_byte(con, '@');

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0001);
    /* we should interrupt at the start of this instruction */
    pdp8->core[01002] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);

    pdp8->core[1] = KSF;
    pdp8->core[2] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[3] = KRB;
    pdp8->core[4] = KSF;
    
    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* SKON */
    pdp8_step(pdp8);        /* first JMP */
    pdp8_step(pdp8);        /* takes interrupt, executes KSF */

    ASSERT_V(callback_mocks.kbd_ready == 0, "kbd_ready not yet called");
    pdp8_step(pdp8);        /* KRB (HLT should be skipped) */
    ASSERT_V(callback_mocks.kbd_ready == 1, "kbd_ready called");
        
    ASSERT_V(pdp8->pc == 4, "Interrupt routine executed");
    ASSERT_V(pdp8->ac == (0200 | '@'), "Keyboard data received"); 

    pdp8_step(pdp8);        /* KSF (should not skip) */
    ASSERT_V(pdp8->pc == 5, "KSF did not ship after flag cleared");

    pdp8_free(pdp8);
}

DECLARE_TEST(con_KCC, "KCC instruction") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }

    pdp8_console_kbd_byte(con, '@');

    pdp8->core[01000] = KSF;        /* this should skip */
    pdp8->core[01001] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[01002] = KCC;
    pdp8->core[01003] = KSF;        /* this should NOT skip */

    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* KSF */
    
    pdp8->ac = 01234;
    pdp8_step(pdp8);        /* KCC */

    ASSERT_V(pdp8->ac == 0, "KCC cleared AC");

    pdp8_step(pdp8);        /* KSF */

    ASSERT_V(pdp8->pc == 01004, "KSF did not skip");

    pdp8_free(pdp8);
}

DECLARE_TEST(con_KCF, "KCF instruction") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }

    pdp8_console_kbd_byte(con, '@');

    pdp8->core[01000] = KSF;        /* this should skip */
    pdp8->core[01001] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[01002] = KCF;
    pdp8->core[01003] = KSF;        /* this should NOT skip */

    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* KSF */
    
    pdp8->ac = 01234;
    pdp8_step(pdp8);        /* KCF */

    ASSERT_V(pdp8->ac == 01234, "KCF did not clear AC");

    pdp8_step(pdp8);        /* KSF */

    ASSERT_V(pdp8->pc == 01004, "KSF did not skip");

    pdp8_free(pdp8);
}

DECLARE_TEST(con_KRS, "KRS instruction") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }

    pdp8_console_kbd_byte(con, '@');

    pdp8->core[01000] = KSF;        /* this should skip */
    pdp8->core[01001] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[01002] = KRS;
    pdp8->core[01003] = KSF;        /* this should skip (KRS doesn't clear the flag) */

    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* KSF */
    
    pdp8->ac = 02000;
    pdp8_step(pdp8);        /* KRS */

    ASSERT_V(pdp8->ac == (02200 | '@'), "KRS or'ed character into AC");

    pdp8_step(pdp8);        /* KSF */

    ASSERT_V(pdp8->pc == 01005, "KSF did skip");

    pdp8_free(pdp8);
}

DECLARE_TEST(con_KIE, "KIE instruction") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);
    pdp8->core[01002] = PDP8_OPR_GRP1 | PDP8_OPR_CLA;
    pdp8->core[01003] = KIE;    
    pdp8->core[01004] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0005);
    pdp8->core[01005] = PDP8_OPR_GRP1 | PDP8_OPR_CLA | PDP8_OPR_GRP1_IAC;
    pdp8->core[01006] = KIE;    
    pdp8->core[01007] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0010);
    
    pdp8->core[1] = KSF;
    pdp8->core[2] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[3] = KRB;
    pdp8->core[4] = KSF;
    
    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* ION */
    pdp8_step(pdp8);        /* JMP */
    pdp8_step(pdp8);        /* CLA */
    pdp8_step(pdp8);        /* KIE */

    /* interrupts are now enabled on the CPU but off on the TTY */
    pdp8_console_kbd_byte(con, '@');

    pdp8_step(pdp8);        /* JMP */

    ASSERT_V(pdp8->pc == 01005, "did not interrupt with KIE disabled");

    pdp8_step(pdp8);        /* CLA IAC */
    pdp8_step(pdp8);        /* KIE */
    pdp8_step(pdp8);        /* KSF */

    ASSERT_V(pdp8->core[0] == 01007, "Interrupt return address stored");
    ASSERT_V(pdp8->pc == 00003, "Interrupt taken");

    pdp8_free(pdp8);
}

DECLARE_TEST(con_TLF_TSF_TCF, "TFL/TSF/TCF instructions") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);
    pdp8->core[01002] = TFL;
    pdp8->core[01003] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);
        
    pdp8->core[1] = TSF;
    pdp8->core[3] = TCF;
    pdp8->core[4] = TSF;
    
    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* ION */
    pdp8_step(pdp8);        /* JMP */
    pdp8_step(pdp8);        /* TLF */
    pdp8_step(pdp8);        /* (interrupt taken) TSF */

    ASSERT_V(pdp8->core[0] == 01003, "interrupt taken");
    ASSERT_V(pdp8->pc == 00003, "TSF executed in interrupt handler");

    pdp8_step(pdp8);        /* TCF */
    pdp8_step(pdp8);        /* TSF (should NOT skip) */
    ASSERT_V(pdp8->pc == 00005, "TSF didn't skip with flag cleared");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(con_TPC_TLS, "TPC/TLS instructions") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }
    
    pdp8->core[01000] = TPC;
    pdp8->core[01001] = TSF;
    pdp8->core[01003] = TCF;
    pdp8->core[01004] = TLS;
    pdp8->core[01005] = TSF;
    
    pdp8->pc = 01000;
    pdp8->ac = '@';

    callback_mocks.printed = 0;

    pdp8_step(pdp8);        /* TPC */

    ASSERT_V(callback_mocks.printed == '@', "Character printed");

    pdp8_step(pdp8);        /* TSF */
    ASSERT_V(pdp8->pc == 01003, "TSF skipped");

    callback_mocks.printed = 0;
    
    pdp8_step(pdp8);        /* TCF */
    pdp8_step(pdp8);        /* TLS */

    ASSERT_V(callback_mocks.printed == '@', "Character printed");

    pdp8_step(pdp8);        /* TSF */
    ASSERT_V(pdp8->pc == 01007, "TSF skipped");
    
    pdp8_free(pdp8);
}   

DECLARE_TEST(con_KIE_prt, "KIE instruction with printer") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }

    pdp8->core[01000] = PDP8_ION;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0002);
    pdp8->core[01002] = PDP8_OPR_GRP1 | PDP8_OPR_CLA;
    pdp8->core[01003] = KIE;
    pdp8->core[01004] = TFL;    
    pdp8->core[01005] = PDP8_OPR_GRP1 | PDP8_OPR_CLA | PDP8_OPR_GRP1_IAC;
    pdp8->core[01006] = KIE;    
    pdp8->core[01007] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE, 0010);
    
    pdp8->core[1] = TSF;
    pdp8->core[2] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    
    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* ION */
    pdp8_step(pdp8);        /* JMP */
    pdp8_step(pdp8);        /* CLA */
    pdp8_step(pdp8);        /* KIE */

    /* interrupts are now enabled on the CPU but off on the TTY */

    pdp8_step(pdp8);        /* TFL */
    pdp8_step(pdp8);        /* CLA IAC */    

    ASSERT_V(pdp8->pc == 01006, "did not interrupt with KIE disabled");

    pdp8_step(pdp8);        /* KIE */
    pdp8_step(pdp8);        /* TSF */
    
    ASSERT_V(pdp8->core[0] == 01007, "Interrupt return address stored");
    ASSERT_V(pdp8->pc == 00003, "Interrupt taken");

    pdp8_free(pdp8);
}

DECLARE_TEST(con_TSK, "TSK instruction") {
    pdp8_t *pdp8 = NULL;
    pdp8_console_t *con = NULL;
    callback_mock_t callback_mocks;

    int ret = setup(&callback_mocks, &pdp8, &con);
    ASSERT_V(ret >= 0, "created and installed console");

    if (ret < 0) {
        return;
    }

    pdp8->core[01000] = PDP8_IOF;
    pdp8->core[01001] = TSK;
    pdp8->core[01002] = TFL;
    pdp8->core[01003] = TSK;
    pdp8->core[01004] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[01005] = TCF;
    pdp8->core[01006] = TSK;
    pdp8->core[01007] = TSK;
    pdp8->core[01010] = PDP8_OPR_GRP2 | PDP8_OPR_GRP2_HLT;
    pdp8->core[01011] = KCF;
    pdp8->core[01012] = TSK;
        
    pdp8->pc = 01000;

    pdp8_step(pdp8);        /* IOF */
    pdp8_step(pdp8);        /* TSK */

    ASSERT_V(pdp8->pc == 01002, "TSK did not skip with no flags set");

    pdp8_step(pdp8);        /* TFL */
    pdp8_step(pdp8);        /* TSK */

    ASSERT_V(pdp8->pc == 01005, "TSK skipped with TTY flag set");

    pdp8_step(pdp8);        /* TCF */
    pdp8_step(pdp8);        /* TSK */

    ASSERT_V(pdp8->pc == 01007, "TSK did not skip after TTY flag cleared");
 
    pdp8_console_kbd_byte(con, '@');
    pdp8_step(pdp8);        /* TSK */

    ASSERT_V(pdp8->pc == 01011, "TSK skipped with keyboard flag set");

    pdp8_step(pdp8);        /* KCF */
    pdp8_step(pdp8);        /* TSK */
    
    ASSERT_V(pdp8->pc == 01013, "TSK did not skip after keyboard flag cleared");

    pdp8_free(pdp8);
}