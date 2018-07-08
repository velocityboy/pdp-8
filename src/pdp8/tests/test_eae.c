#include <stdio.h>
#include "tests.h"
#include "pdp8/emulator.h"

DECLARE_TEST(eae_disabled, "EAE ops are NOP if option is not installed") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_GRP3_SCA;
    pdp8->ac = 00000;
    pdp8->sc = 00037;
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00000, "AC not changed");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_not_on_8l, "EAE is not supported on the PDP-8/L") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8_set_model(pdp8, PDP8_L);

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_GRP3_SCA;
    pdp8->ac = 00000;
    pdp8->sc = 00037;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00000, "AC not changed");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_CLA, "EAE group CLA") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    /* CLA works even if the EAE option is not installed. */
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00000, "AC cleared");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_MQA, "EAE group MQA") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_GRP3_MQA;
    pdp8->ac = 00507;
    pdp8->mq = 00222;
    pdp8->pc = 01000;

    /* MQA works even if the EAE option is not installed. */
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00727, "AC or'ed with MQ");
    ASSERT_V(pdp8->mq == 00222, "MQ not changed");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_CLA_MQA, "EAE group CLA MQA") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_OPR_GRP3_MQA;
    pdp8->ac = 00507;
    pdp8->mq = 00222;
    pdp8->pc = 01000;

    /* MQA works even if the EAE option is not installed. */
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00222, "AC loaded from MQ");
    ASSERT_V(pdp8->mq == 00222, "MQ not changed");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_MQL, "EAE group MQL") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_GRP3_MQL;
    pdp8->ac = 00507;
    pdp8->mq = 00222;
    pdp8->pc = 01000;

    /* MQL works even if the EAE option is not installed. */
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00000, "AC cleared");
    ASSERT_V(pdp8->mq == 00507, "MQ loaded from AC");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_CLA_MQL, "EAE group CLA MQL") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_OPR_GRP3_MQL;
    pdp8->ac = 00507;
    pdp8->mq = 00222;
    pdp8->pc = 01000;

    /* MQL works even if the EAE option is not installed. */
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00000, "AC cleared");
    ASSERT_V(pdp8->mq == 00000, "MQ cleared");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_SCA, "EAE group SCA") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_GRP3_SCA;
    pdp8->ac = 00700;
    pdp8->sc = 00037;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00737, "AC or'ed with SC");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_CLA_SCA, "EAE group CLA SCA") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_OPR_GRP3_SCA;
    pdp8->ac = 00700;
    pdp8->sc = 00037;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00037, "AC loaded from SC");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_SCL, "EAE group SCL") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_SCL;
    pdp8->core[01001] = 05212;
    pdp8->sc = 00000;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->sc == 00025, "SC loaded from memory");
    ASSERT_V(pdp8->pc == 01002, "PC properly incremented past memory operand");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_SCL_support, "EAE SCL unsupported on straight eight") {
    pdp8_t *pdp8 = pdp8_create();
    pdp8_set_model(pdp8, PDP8);

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_SCL;
    pdp8->core[01001] = 05212;
    pdp8->sc = 00000;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->run == 0, "CPU halted on SCL on PDP8");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_SCL_UNSUPPORTED, "HALT reason set on SCL on PDP8");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_MUY, "EAE group MUY") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_MUY;
    pdp8->core[01001] = 07421;
    pdp8->mq = 06543;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 06233, "AC contains high bits of product");
    ASSERT_V(pdp8->mq == 00223, "MQ contains low bits of product");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past memory operand");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_DVI, "EAE group DVI") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_DVI;
    pdp8->core[01001] = 03412;
    pdp8->ac = 01234;
    pdp8->mq = 05677;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00137, "AC contains the remainder");
    ASSERT_V(pdp8->mq == 02760, "MQ contains the quotient");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past memory operand");
    
    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_DVI;
    pdp8->core[01001] = 00001;
    pdp8->ac = 07777;
    pdp8->mq = 07777;
    pdp8->link = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK was set on overflow");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past memory operand");

    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_DVI;
    pdp8->core[01001] = 00000;
    pdp8->ac = 07777;
    pdp8->mq = 07777;
    pdp8->link = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK was set on overflow");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past memory operand");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_NMI, "EAE group NMI") {
    pdp8_t *pdp8 = pdp8_create();

    /* shifts until top two bits of AC are different */
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_NMI;
    pdp8->link = 1;
    pdp8->ac = 00527;
    pdp8->mq = 03427;
    pdp8->sc = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK was shifted");
    ASSERT_V(pdp8->ac == 02535, "AC was shifted");
    ASSERT_V(pdp8->mq == 06134, "MQ was shifted");
    ASSERT_V(pdp8->sc == 002, "SC contains number of shifts");
    ASSERT_V(pdp8->pc == 01001, "PC incremented");

    /* shifts until AC|MQ is 6000 0000 */
    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_NMI;
    pdp8->link = 0;
    pdp8->ac = 07600;
    pdp8->mq = 00000;
    pdp8->sc = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK was shifted");
    ASSERT_V(pdp8->ac == 06000, "AC was shifted");
    ASSERT_V(pdp8->mq == 00000, "MQ was shifted");
    ASSERT_V(pdp8->sc == 003, "SC contains number of shifts");
    ASSERT_V(pdp8->pc == 01001, "PC incremented");

    /* does not shift if AC|MQ is 0000 0000 (older hardware hangs in this case) */
    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_NMI;
    pdp8->link = 0;
    pdp8->ac = 00000;
    pdp8->mq = 00000;
    pdp8->sc = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK was unchanged");
    ASSERT_V(pdp8->ac == 00000, "AC was unchanged");
    ASSERT_V(pdp8->mq == 00000, "MQ was unchanged");
    ASSERT_V(pdp8->sc == 000, "SC contains number of shifts");
    ASSERT_V(pdp8->pc == 01001, "PC incremented");
    
    /* in mode A, a result of 4000 0000 should NOT be modified */
    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_NMI;
    pdp8->link = 0;
    pdp8->ac = 04000;
    pdp8->mq = 00000;
    pdp8->sc = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK was unchanged");
    ASSERT_V(pdp8->ac == 04000, "AC was unchanged");
    ASSERT_V(pdp8->mq == 00000, "MQ was unchanged");
    ASSERT_V(pdp8->sc == 000, "SC contains number of shifts");
    ASSERT_V(pdp8->pc == 01001, "PC incremented");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_SHL, "EAE group SHL") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_SHL;
    pdp8->core[01001] = 01002; /* NB shifts 1 more than the low 5 bits == 3 */
    pdp8->link = 0;
    pdp8->ac = 01123;
    pdp8->mq = 04567;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK was shifted");
    ASSERT_V(pdp8->ac == 01234, "AC was shifted");
    ASSERT_V(pdp8->mq == 05670, "MQ was shifted");
    ASSERT_V(pdp8->pc == 01002, "PC properly incremented past memory operand");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_LSR, "EAE group LSR") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_LSR;
    pdp8->core[01001] = 01002; /* NB shifts 1 more than the low 5 bits == 3 */
    pdp8->link = 1;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->ac == 00770, "AC was shifted");
    ASSERT_V(pdp8->mq == 04001, "MQ was shifted");
    ASSERT_V(pdp8->pc == 01002, "PC properly incremented past memory operand");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_ASR, "EAE group ASR") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_ASR;
    pdp8->core[01001] = 01002; /* NB shifts 1 more than the low 5 bits == 3 */
    pdp8->link = 0;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK was set");
    ASSERT_V(pdp8->ac == 07770, "AC was shifted");
    ASSERT_V(pdp8->mq == 04001, "MQ was shifted");
    ASSERT_V(pdp8->pc == 01002, "PC properly incremented past memory operand");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_SWP, "EAE group SWP") {
    pdp8_t *pdp8 = pdp8_create();
    
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_GRP3_MQA | PDP8_OPR_GRP3_MQL;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;

    /* this operation works even if the EAE option is not installed. */
    
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00017, "AC was swapped");
    ASSERT_V(pdp8->mq == 07704, "MQ was swapped");

    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_OPR_GRP3_MQA | PDP8_OPR_GRP3_MQL;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;

    /* this operation works even if the EAE option is not installed. */
    
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00017, "AC was swapped");
    ASSERT_V(pdp8->mq == 00000, "MQ was cleared");
        
    pdp8_free(pdp8);        
}

DECLARE_TEST(eae_swp_not_on_8_8s, "EAE SWP not supported on PDP-8 or PDP-8/S") {
    pdp8_t *pdp8 = pdp8_create();
    pdp8_set_model(pdp8, PDP8);
    
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_GRP3_MQA | PDP8_OPR_GRP3_MQL;
    pdp8->mq = 07704;
    pdp8->ac = 00017;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->run == 0, "machine was halted");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_SWP_UNSUPPORTED, "halt reason set");

    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_S);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_OPR_GRP3_MQA | PDP8_OPR_GRP3_MQL;
    pdp8->mq = 07704;
    pdp8->ac = 00017;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->run == 0, "machine was halted");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_SWP_UNSUPPORTED, "halt reason set");
        
    pdp8_free(pdp8);        
    
}

DECLARE_TEST(eae_sequencing, "EAE group sequencing") {
    /* CLA comes before MQA, SCA, and MQL; this is tested above 
     * here we will test MQA => instr code 
     */
    pdp8_t *pdp8 = pdp8_create();
     
    /* 0 -> AC, then MQ|AC -> AC, then SHL */
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_OPR_GRP3_MQA | PDP8_GRP3_CODE_SHL;
    pdp8->core[01001] = 01002; /* NB shifts 1 more than the low 5 bits == 3 */
    pdp8->link = 0;
    pdp8->ac = 01234;
    pdp8->mq = 07654;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
     
    pdp8_step(pdp8);
     
    ASSERT_V(pdp8->link == 1, "LINK was set");
    ASSERT_V(pdp8->ac == 06547, "AC was shifted");
    ASSERT_V(pdp8->mq == 06540, "MQ was shifted");
    ASSERT_V(pdp8->pc == 01002, "PC properly incremented past memory operand");
         
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_cli_nmi_hang, "EAE CLA+NMI with AC != 0 hangs on early machines") {
    pdp8_t *pdp8 = pdp8_create();
    pdp8_set_model(pdp8, PDP8);    

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_GRP3_CODE_NMI;
    pdp8->link = 0;
    pdp8->ac = 01234;
    pdp8->mq = 07654;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->run == 0, "CPU halted");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_CLA_NMI_UNSUPPORTED, "HALT reasons set");

    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_S);    
    
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_GRP3_CODE_NMI;
    pdp8->link = 0;
    pdp8->ac = 01234;
    pdp8->mq = 07654;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;   

    pdp8_step(pdp8);    

    ASSERT_V(pdp8->run == 0, "CPU halted");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_CLA_NMI_UNSUPPORTED, "HALT reasons set");
    
    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_I);    
    
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_GRP3_CODE_NMI;
    pdp8->link = 0;
    pdp8->ac = 01234;
    pdp8->mq = 07654;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;   

    pdp8_step(pdp8);    

    ASSERT_V(pdp8->run == 0, "CPU halted");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_CLA_NMI_UNSUPPORTED, "HALT reasons set");
    
    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_L);    
    
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_OPR_CLA | PDP8_GRP3_CODE_NMI;
    pdp8->link = 0;
    pdp8->ac = 01234;
    pdp8->mq = 07654;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;   

    pdp8_step(pdp8);    

    ASSERT_V(pdp8->run == 0, "CPU halted");
    ASSERT_V(pdp8->halt_reason == PDP8_HALT_CLA_NMI_UNSUPPORTED, "HALT reasons set");
    
    pdp8_free(pdp8);
}

/*-----------------------------------------------------------------------------
 *
 * Below tests instructions that only work in mode B (more modern EAE 
 * implementations).
 *
 */

DECLARE_TEST(eae_mode_switch, "EAE mode switching") {
    pdp8_t *pdp8 = pdp8_create();
    
    /* mode B is not available on the straight 8, 8/S, 8/I, 8/L */
    pdp8_set_model(pdp8, PDP8);
     
    pdp8->core[01000] = PDP8_SWAB;
    pdp8->option_eae = 1;
    pdp8->pc = 01000;
     
    pdp8_step(pdp8);
     
    ASSERT_V(pdp8->eae_mode_b == 0, "SWAB ignored on PDP-8");

    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_S);
    
    pdp8->core[01000] = PDP8_SWAB;
    pdp8->option_eae = 1;
    pdp8->pc = 01000;
    
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->eae_mode_b == 0, "SWAB ignored on PDP-8/S");

    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_I);
    
    pdp8->core[01000] = PDP8_SWAB;
    pdp8->option_eae = 1;
    pdp8->pc = 01000;
    
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->eae_mode_b == 0, "SWAB ignored on PDP-8/I");

    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_L);
    
    pdp8->core[01000] = PDP8_SWAB;
    pdp8->option_eae = 1;
    pdp8->pc = 01000;
    
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->eae_mode_b == 0, "SWAB ignored on PDP-8/L");

    /* should work on the PDP-8/E */
    pdp8_clear(pdp8);
    pdp8_set_model(pdp8, PDP8_E);
    pdp8->core[01000] = PDP8_SWAB;
    pdp8->option_eae = 1;
    pdp8->pc = 01000;    
    
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->eae_mode_b == 1, "SWAB sets mode B on PDP-8/E");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_SWBA;
    pdp8->eae_mode_b = 1;
    pdp8->option_eae = 1;
    pdp8->pc = 01000;
    
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->eae_mode_b == 0, "SWBA clears mode B on PDP-8/E");
   
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_ACS, "EAE group mode B ACS") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_ACS;
    pdp8->ac = 00707;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00000, "AC cleared");
    ASSERT_V(pdp8->sc == 00007, "SC set to low 5 bits of AC");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_MUY, "EAE group mode B MUY") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_MUY;
    pdp8->core[01001] = 02000;
    pdp8->core[02000] = 07421;
    pdp8->mq = 06543;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 06233, "AC contains high bits of product");
    ASSERT_V(pdp8->mq == 00223, "MQ contains low bits of product");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past memory operand");
    
    /* if the code is in the auto-increment region, auto-increment applies */
    pdp8_clear(pdp8);
    pdp8->core[00010] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_MUY;
    pdp8->core[00011] = 02000;
    pdp8->core[02001] = 07421;
    pdp8->mq = 06543;
    pdp8->link = 1;
    pdp8->pc = 00010;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->core[00011] == 02001, "Auto-increment was applied");
    ASSERT_V(pdp8->ac == 06233, "AC contains high bits of product");
    ASSERT_V(pdp8->mq == 00223, "MQ contains low bits of product");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 00012, "PC incremented past memory operand");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_DVI, "EAE group B DVI") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DVI;
    pdp8->core[01001] = 02000;
    pdp8->core[02000] = 03412;
    pdp8->ac = 01234;
    pdp8->mq = 05677;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00137, "AC contains the remainder");
    ASSERT_V(pdp8->mq == 02760, "MQ contains the quotient");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past memory operand");

    pdp8_clear(pdp8);
    pdp8->core[00010] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DVI;
    pdp8->core[00011] = 02000;
    pdp8->core[02001] = 03412;
    pdp8->ac = 01234;
    pdp8->mq = 05677;
    pdp8->link = 1;
    pdp8->pc = 00010;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00137, "AC contains the remainder");
    ASSERT_V(pdp8->mq == 02760, "MQ contains the quotient");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 00012, "PC incremented past memory operand");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_NMI, "EAE group B NMI") {
    pdp8_t *pdp8 = pdp8_create();

    /* shifts until top two bits of AC are different */
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_NMI;
    pdp8->link = 1;
    pdp8->ac = 00527;
    pdp8->mq = 03427;
    pdp8->sc = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK was shifted");
    ASSERT_V(pdp8->ac == 02535, "AC was shifted");
    ASSERT_V(pdp8->mq == 06134, "MQ was shifted");
    ASSERT_V(pdp8->sc == 002, "SC contains number of shifts");
    ASSERT_V(pdp8->pc == 01001, "PC incremented");

    /* shifts until AC|MQ is 6000 0000 */
    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_NMI;
    pdp8->link = 0;
    pdp8->ac = 07600;
    pdp8->mq = 00000;
    pdp8->sc = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK was shifted");
    ASSERT_V(pdp8->ac == 06000, "AC was shifted");
    ASSERT_V(pdp8->mq == 00000, "MQ was shifted");
    ASSERT_V(pdp8->sc == 003, "SC contains number of shifts");
    ASSERT_V(pdp8->pc == 01001, "PC incremented");

    /* does not shift if AC|MQ is 0000 0000 (older hardware hangs in this case) */
    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_NMI;
    pdp8->link = 0;
    pdp8->ac = 00000;
    pdp8->mq = 00000;
    pdp8->sc = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK was unchanged");
    ASSERT_V(pdp8->ac == 00000, "AC was unchanged");
    ASSERT_V(pdp8->mq == 00000, "MQ was unchanged");
    ASSERT_V(pdp8->sc == 000, "SC contains number of shifts");
    ASSERT_V(pdp8->pc == 01001, "PC incremented");
    
    /* in mode B, a result of 4000 0000 should clear AC */
    pdp8_clear(pdp8);
    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_NMI;
    pdp8->link = 0;
    pdp8->ac = 04000;
    pdp8->mq = 00000;
    pdp8->sc = 0;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK was unchanged");
    ASSERT_V(pdp8->ac == 00000, "AC was cleared");
    ASSERT_V(pdp8->mq == 00000, "MQ was unchanged");
    ASSERT_V(pdp8->sc == 000, "SC contains number of shifts");
    ASSERT_V(pdp8->pc == 01001, "PC incremented");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_SHL, "EAE group B SHL") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_SHL;
    pdp8->core[01001] = 01003;
    pdp8->link = 0;
    pdp8->ac = 01123;
    pdp8->mq = 04567;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK was shifted");
    ASSERT_V(pdp8->ac == 01234, "AC was shifted");
    ASSERT_V(pdp8->mq == 05670, "MQ was shifted");
    ASSERT_V(pdp8->pc == 01002, "PC properly incremented past memory operand");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_LSR, "EAE group B LSR") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_LSR;
    pdp8->core[01001] = 01003;
    pdp8->link = 1;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->ac == 00770, "AC was shifted");
    ASSERT_V(pdp8->mq == 04001, "MQ was shifted");
    ASSERT_V(pdp8->pc == 01002, "PC properly incremented past memory operand");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_ASR, "EAE group B ASR") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_ASR;
    pdp8->core[01001] = 01003;
    pdp8->link = 0;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->link == 1, "LINK was set");
    ASSERT_V(pdp8->ac == 07770, "AC was shifted");
    ASSERT_V(pdp8->mq == 04001, "MQ was shifted");
    ASSERT_V(pdp8->pc == 01002, "PC properly incremented past memory operand");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_SCA, "EAE group B SCA") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_SCA;
    pdp8->ac = 00700;
    pdp8->sc = 00037;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00737, "AC or'ed with SC");

    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_DAD, "EAE group B DAD") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DAD;
    pdp8->core[01001] = 02000;
    pdp8->core[02000] = 01111; /* low */
    pdp8->core[02001] = 02222; /* high */
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 06666, "AC is high word of sum");
    ASSERT_V(pdp8->mq == 04444, "MQ is high word of sum");
    ASSERT_V(pdp8->link == 0, "LINK is cleared");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past data");

    pdp8_clear(pdp8);

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DAD;
    pdp8->core[01001] = 02000;
    pdp8->core[02000] = 01111; /* low */
    pdp8->core[02001] = 04222; /* high */
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 00666, "AC is high word of sum");
    ASSERT_V(pdp8->mq == 04444, "MQ is high word of sum");
    ASSERT_V(pdp8->link == 1, "LINK is set");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past data");
    
    pdp8_clear(pdp8);

    pdp8->core[00010] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DAD;
    pdp8->core[00011] = 02000;
    pdp8->core[02001] = 01111; /* low */
    pdp8->core[02002] = 02222; /* high */
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->link = 1;
    pdp8->pc = 00010;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->ac == 06666, "AC is high word of sum");
    ASSERT_V(pdp8->mq == 04444, "MQ is high word of sum");
    ASSERT_V(pdp8->link == 0, "LINK is cleared");
    ASSERT_V(pdp8->pc == 00012, "PC incremented past data");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(eae_B_DDST, "EAE group B DST") {
    pdp8_t *pdp8 = pdp8_create();

    pdp8->core[01000] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DST;
    pdp8->core[01001] = 02000;
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->core[02000] == 03333, "low word stored");
    ASSERT_V(pdp8->core[02001] == 04444, "low word stored");
    ASSERT_V(pdp8->pc == 01002, "PC incremented past data");

    pdp8_clear(pdp8);

    pdp8->core[00010] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DST;
    pdp8->core[00011] = 02000;
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->link = 1;
    pdp8->pc = 00010;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->core[02001] == 03333, "low word stored");
    ASSERT_V(pdp8->core[02002] == 04444, "low word stored");
    ASSERT_V(pdp8->pc == 00012, "PC incremented past data");
    
    pdp8_free(pdp8);
}

