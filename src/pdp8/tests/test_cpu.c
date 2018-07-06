#include <stdio.h>
#include "tests.h"

DECLARE_TEST(cpu_TAD, "TAD instruction works") {
    ASSERT(1 == 2);
    ASSERT_V(3 == 4, "auto-increment pulls from correct address");
}

DECLARE_TEST(cpu_JMP, "JMP instruction works") {
}
