#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdp8/devices.h"

#include "options.h"

typedef struct option_t option_t;

struct option_t {
    char *name;
    void (*handler)(pdp8_t *pdp8);    
};

static void enable_eae(pdp8_t *pdp8);
static void enable_mex(pdp8_t *pdp8);
static void enable_8e(pdp8_t *pdp8);

static option_t options[] = {
    { "eae", &enable_eae },
    { "mex", &enable_mex },
    { "8e", &enable_8e },
    { NULL, NULL},
};

static char **option_names = NULL;

char **get_option_names() {
    if (option_names == NULL) {
        int i = 0;
        for (; options[i].name; i++) {            
        }

        option_names = (char**)calloc(i + 1, sizeof(char*));

        for (i = 0; options[i].name; i++) {
            option_names[i] = options[i].name;
        }
    }
    return option_names;
}

int enable_option(char *name, pdp8_t *pdp8) {
    for (int i = 0; options[i].name; i++) {
        if (strcasecmp(name, options[i].name) == 0) {
            options[i].handler(pdp8);
            return 0;
        }
    }
    return -1;
}

void enable_eae(pdp8_t *pdp8) {
    pdp8->option_eae = 1;
}

void enable_mex(pdp8_t *pdp8) {
    int ret = pdp8_install_mex_tso(pdp8);
    if (ret < 0) {
        printf("failed to install memory controller (%d)\n", ret);
        return;
    }

    ret = pdp8_set_mex_fields(pdp8, 8);
    if (ret < 0) {
        printf("failed to install memory (%d)\n", ret);
    }
}

void enable_8e(pdp8_t *pdp8) {
    int ret = pdp8_set_model(pdp8, PDP8_E);
    if (ret < 0) {
        printf("failed to upgrade model to PDP8/E (%d)\n", ret);
    }
}
