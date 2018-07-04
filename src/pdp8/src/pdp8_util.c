#include <string.h>

#include "pdp8/defines.h"
#include "pdp8/emulator.h"

pdp8_reg_t pdp8_reg_from_string(char *s) {
    if (strcasecmp(s, "AC") == 0) {
        return REG_AC;
    } else if (strcasecmp(s, "PC") == 0) {
        return REG_PC;
    } else if (strcasecmp(s, "LINK") == 0 || strcasecmp(s, "L") == 0) {
        return REG_LINK;
    } else if (strcasecmp(s, "RUN") == 0 || strcasecmp(s, "R") == 0) {
        return REG_RUN;
    } else if (strcasecmp(s, "SR") == 0) {
        return REG_SR;
    }

    return REG_INVALID;
}
