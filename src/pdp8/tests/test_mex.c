#include <stdio.h>
#include "tests.h"
#include "pdp8/devices.h"

DECLARE_TEST(mex_CDF, "MEX change data field") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->pc = 01000;

    pdp8_step(pdp8);

    ASSERT_V(pdp8->dfr == 040000, "DF register set");

    pdp8_free(pdp8);
}

DECLARE_TEST(mex_RDF, "MEX read data field") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    
    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = MEX_RDF;
    pdp8->ac = 05505;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->dfr == 040000, "DF register set");
    ASSERT_V(pdp8->ac == 05545, "DF or'ed into AC");

    pdp8_free(pdp8);
}

DECLARE_TEST(mex_CDF_indirects, "MEX CDF works with indirect memory instructions") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_AND, PDP8_M_INDIRECT, 00020);
    pdp8->core[00020] = 00030;
    pdp8->core[040030]= 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 01002, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00700, "AND result correct");

    pdp8_clear(pdp8);
    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_TAD, PDP8_M_INDIRECT, 00020);
    pdp8->core[00020] = 00030;
    pdp8->core[040030]= 00007;
    pdp8->ac =          00007;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 01002, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00016, "ADD with no carry correct");
    ASSERT_V(pdp8->link == 0, "LINK is not affected with no carry");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_ISZ, PDP8_M_INDIRECT, 00020);
    pdp8->core[00020] = 00030;
    pdp8->core[040030]= 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 01002, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00707, "ISZ does not affect AC");
    ASSERT_V(pdp8->core[040030] == 07701, "ISZ increments memory");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_DCA, PDP8_M_INDIRECT, 00020);
    pdp8->core[00020] = 0030;
    pdp8->core[040030]= 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 01002, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00000, "DCA clears AC");
    ASSERT_V(pdp8->core[040030] == 00707, "DCA stores contents of AC");

    /* DF should not affect JMS or JMP */
    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMS, PDP8_M_PAGE|PDP8_M_INDIRECT, 00020);
    pdp8->core[01020] = 01030;

    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 01031, "instruction pointer updated");
    ASSERT_V(pdp8->core[01030] == 01002, "return address stored");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, PDP8_M_PAGE|PDP8_M_INDIRECT, 00020);
    pdp8->core[01020] = 01030;

    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 01030, "instruction pointer updated");

    pdp8_free(pdp8);
}

DECLARE_TEST(mex_CDF_EAE, "MEX CDF works with EAE") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_MUY;
    pdp8->core[01002] = 02000;
    pdp8->core[042000]= 07421;
    pdp8->mq = 06543;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 06233, "AC contains high bits of product");
    ASSERT_V(pdp8->mq == 00223, "MQ contains low bits of product");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 01003, "PC incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DVI;
    pdp8->core[01002] = 02000;
    pdp8->core[042000] = 03412;
    pdp8->ac = 01234;
    pdp8->mq = 05677;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00137, "AC contains the remainder");
    ASSERT_V(pdp8->mq == 02760, "MQ contains the quotient");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 01003, "PC incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DAD;
    pdp8->core[01002] = 02000;
    pdp8->core[042000]= 01111; /* low */
    pdp8->core[042001]= 02222; /* high */
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 06666, "AC is high word of sum");
    ASSERT_V(pdp8->mq == 04444, "MQ is high word of sum");
    ASSERT_V(pdp8->link == 0, "LINK is cleared");
    ASSERT_V(pdp8->pc == 01003, "PC incremented past data");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 4);
    pdp8->core[01001] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DST;
    pdp8->core[01002] = 02000;
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->core[042000] == 03333, "low word stored");
    ASSERT_V(pdp8->core[042001] == 04444, "low word stored");
    ASSERT_V(pdp8->pc == 01003, "PC incremented past data");

    pdp8_free(pdp8);
}

DECLARE_TEST(mex_CIF, "MEX change instruction field") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    
    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->intr_enable_mask = PDP8_INTR_ION;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    ASSERT_V(pdp8->ibr == 040000, "IB register set");
    ASSERT_V(pdp8->ifr == 000000, "IF register not yet changed");
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts are disabled");
    
    pdp8_step(pdp8);
    ASSERT_V(pdp8->ifr == 040000, "IF register set");
    ASSERT_V(pdp8->pc == 00020, "PC set");
    ASSERT_V(pdp8_interrupts_enabled(pdp8), "Interrupts are enabled");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(mex_CIF_indirects, "MEX CIF works with memory instructions") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_M_MAKE(PDP8_OP_AND, 0, 00021);
    pdp8->core[040021]= 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 00021, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00700, "AND result correct");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_M_MAKE(PDP8_OP_TAD, 0, 00021);
    pdp8->core[040021]= 00007;
    pdp8->ac =          00007;
    pdp8->link = 0;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 00021, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00016, "ADD with no carry correct");
    ASSERT_V(pdp8->link == 0, "LINK is not affected with no carry");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_M_MAKE(PDP8_OP_ISZ, 0, 00021);
    pdp8->core[040021]= 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 00021, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00707, "ISZ does not affect AC");
    ASSERT_V(pdp8->core[040021] == 07701, "ISZ increments memory");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_M_MAKE(PDP8_OP_DCA, 0, 00021);
    pdp8->core[040021]= 07700;
    pdp8->ac =          00707;
    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 00021, "instruction pointer incremented");
    ASSERT_V(pdp8->ac == 00000, "DCA clears AC");
    ASSERT_V(pdp8->core[040021] == 00707, "DCA stores contents of AC");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_M_MAKE(PDP8_OP_JMS, 0, 00030);

    pdp8->pc = 01000;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->pc == 00031, "instruction pointer updated");
    ASSERT_V(pdp8->core[040030] == 00021, "return address stored");

    pdp8_free(pdp8);
}

DECLARE_TEST(mex_CIF_EAE, "MEX CIF works with EAE") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    
    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_MUY;
    pdp8->core[040021]= 07421;
    pdp8->mq = 06543;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 06233, "AC contains high bits of product");
    ASSERT_V(pdp8->mq == 00223, "MQ contains low bits of product");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 00022, "PC incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DVI;
    pdp8->core[040021] = 03412;
    pdp8->ac = 01234;
    pdp8->mq = 05677;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00137, "AC contains the remainder");
    ASSERT_V(pdp8->mq == 02760, "MQ contains the quotient");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 00022, "PC incremented past memory operand");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_SHL;
    pdp8->core[040021]= 01002; /* NB shifts 1 more than the low 5 bits == 3 */
    pdp8->link = 0;
    pdp8->ac = 01123;
    pdp8->mq = 04567;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->link == 1, "LINK was shifted");
    ASSERT_V(pdp8->ac == 01234, "AC was shifted");
    ASSERT_V(pdp8->mq == 05670, "MQ was shifted");
    ASSERT_V(pdp8->pc == 00022, "PC properly incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_LSR;
    pdp8->core[040021]= 01002; /* NB shifts 1 more than the low 5 bits == 3 */
    pdp8->link = 1;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->ac == 00770, "AC was shifted");
    ASSERT_V(pdp8->mq == 04001, "MQ was shifted");
    ASSERT_V(pdp8->pc == 00022, "PC properly incremented past memory operand");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_ASR;
    pdp8->core[040021]= 01002; /* NB shifts 1 more than the low 5 bits == 3 */
    pdp8->link = 0;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->link == 1, "LINK was set");
    ASSERT_V(pdp8->ac == 07770, "AC was shifted");
    ASSERT_V(pdp8->mq == 04001, "MQ was shifted");
    ASSERT_V(pdp8->pc == 00022, "PC properly incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 2);
    pdp8->core[01001] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01002] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_MUY;
    pdp8->core[040021]= 02000;
    pdp8->core[022000]= 07421;
    pdp8->mq = 06543;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 06233, "AC contains high bits of product");
    ASSERT_V(pdp8->mq == 00223, "MQ contains low bits of product");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 0022, "PC incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 2);
    pdp8->core[01001] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01002] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DVI;
    pdp8->core[040021]= 02000;
    pdp8->core[022000]= 03412;
    pdp8->ac = 01234;
    pdp8->mq = 05677;
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 00137, "AC contains the remainder");
    ASSERT_V(pdp8->mq == 02760, "MQ contains the quotient");
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->pc == 00022, "PC incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_SHL;
    pdp8->core[040021] = 01003;
    pdp8->link = 0;
    pdp8->ac = 01123;
    pdp8->mq = 04567;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->link == 1, "LINK was shifted");
    ASSERT_V(pdp8->ac == 01234, "AC was shifted");
    ASSERT_V(pdp8->mq == 05670, "MQ was shifted");
    ASSERT_V(pdp8->pc == 0022, "PC properly incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_LSR;
    pdp8->core[040021]= 01003;
    pdp8->link = 1;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->link == 0, "LINK was cleared");
    ASSERT_V(pdp8->ac == 00770, "AC was shifted");
    ASSERT_V(pdp8->mq == 04001, "MQ was shifted");
    ASSERT_V(pdp8->pc == 0022, "PC properly incremented past memory operand");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_ASR;
    pdp8->core[040021]= 01003;
    pdp8->link = 0;
    pdp8->ac = 07704;
    pdp8->mq = 00017;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->link == 1, "LINK was set");
    ASSERT_V(pdp8->ac == 07770, "AC was shifted");
    ASSERT_V(pdp8->mq == 04001, "MQ was shifted");
    ASSERT_V(pdp8->pc == 00022, "PC properly incremented past memory operand");
    
    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 2);
    pdp8->core[01001] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01002] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DAD;
    pdp8->core[040021]= 02000;
    pdp8->core[022000]= 01111; /* low */
    pdp8->core[022001]= 02222; /* high */
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->link = 1;
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 06666, "AC is high word of sum");
    ASSERT_V(pdp8->mq == 04444, "MQ is high word of sum");
    ASSERT_V(pdp8->link == 0, "LINK is cleared");
    ASSERT_V(pdp8->pc == 00022, "PC incremented past data");

    pdp8_clear(pdp8);

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF, 2);
    pdp8->core[01001] = MEX_MAKE_OP(MEX_CIF, 4);
    pdp8->core[01002] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020] = PDP8_OPR_GRP3 | PDP8_GRP3_CODE_B_DST;
    pdp8->core[040021] = 02000;
    pdp8->mq = 03333; /* low */
    pdp8->ac = 04444; /* high */
    pdp8->pc = 01000;
    pdp8->option_eae = 1;
    pdp8->eae_mode_b = 1;
    
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->core[022000] == 03333, "low word stored");
    ASSERT_V(pdp8->core[022001] == 04444, "low word stored");
    ASSERT_V(pdp8->pc == 00022, "PC incremented past data");

    pdp8_free(pdp8);
}

DECLARE_TEST(mex_CDF_CIF, "MEX change data field and instr field") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->pc = 01000;
    pdp8->intr_enable_mask = PDP8_INTR_ION;
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->dfr == 040000, "DF register set");
    ASSERT_V(pdp8->ibr == 040000, "IB register set");
    ASSERT_V(pdp8->ifr == 000000, "IF register not yet set");
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts are disabled");
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->dfr == 040000, "DF register set");
    ASSERT_V(pdp8->ibr == 040000, "IB register set");
    ASSERT_V(pdp8->ifr == 040000, "IF register set");
    ASSERT_V(pdp8_interrupts_enabled(pdp8), "Interrupts are enabled");

    pdp8_free(pdp8);
}

DECLARE_TEST(mex_RIF, "MEX read instr field") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    

    pdp8->core[01000] = MEX_MAKE_OP(MEX_CDF_CIF, 4);
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->core[040020]= MEX_RIF;
    pdp8->pc = 01000;
    pdp8->ac = 05515;

    pdp8_step(pdp8);
    pdp8_step(pdp8);
    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 05555, "IF or'ed into AC");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(mex_RIB, "MEX read interrupt buffer") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    
    pdp8->core[01000] = MEX_RIB;
    pdp8->pc = 01000;
    pdp8->ac = 05400;
    pdp8->sf = 00155;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ac == 05555, "SF or'ed into AC");
    
    pdp8_free(pdp8);
}

DECLARE_TEST(mex_RMF, "MEX restore memory field") {
    pdp8_t *pdp8 = pdp8_create();

    int ret = pdp8_install_mex_tso(pdp8);
    ASSERT_V(ret >= 0, "successfully installed MEX hardware");
    
    ret = pdp8_set_mex_fields(pdp8, 8);
    ASSERT_V(ret >= 0, "successfully installed max memory");
    
    pdp8->core[01000] = MEX_RMF;
    pdp8->core[01001] = PDP8_M_MAKE(PDP8_OP_JMP, 0, 00020);
    pdp8->pc = 01000;
    pdp8->sf = 00045;
    pdp8->intr_enable_mask = PDP8_INTR_ION;

    pdp8_step(pdp8);
    
    ASSERT_V(pdp8->ifr == 000000, "IF not yet updated");
    ASSERT_V(pdp8->ibr == 040000, "IB updated");
    ASSERT_V(pdp8->dfr == 050000, "DF updated");
    ASSERT_V(!pdp8_interrupts_enabled(pdp8), "Interrupts are disabled");
    
    pdp8_step(pdp8);

    ASSERT_V(pdp8->pc == 00020, "PC updated");
    ASSERT_V(pdp8->ifr == 040000, "IF updated");
    ASSERT_V(pdp8->ibr == 040000, "IB updated");
    ASSERT_V(pdp8->dfr == 050000, "DF updated");
    ASSERT_V(pdp8_interrupts_enabled(pdp8), "Interrupts enabled");
    
    pdp8_free(pdp8);
}