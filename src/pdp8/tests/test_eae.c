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

