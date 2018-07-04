#include <stdlib.h>
#include <string.h>

#include "options.h"

typedef struct option_t option_t;

struct option_t {
    char *name;
    void (*handler)(pdp8_t *pdp8);    
};

static void enable_eae(pdp8_t *pdp8);

static option_t options[] = {
    { "eae", &enable_eae },
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
