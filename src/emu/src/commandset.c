#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "commandset.h"

/*
 * Execute, either by name or abbreviation, a command in the 
 * given set.
 */
void execute_command(char *command, command_t *command_set) {
    while (isspace(*command)) {
        command++;
    }

    if (*command == '\0') {
        return;
    }

    char *start = command;
    while (*command && !isspace(*command)) {
        command++;
    }

    if (*command) {
        *command++ = '\0';
    }
    char *tail = command;
    

    for (command_t *p = command_set; p->command != NULL; p++) {
        if (strcasecmp(start, p->command) == 0 || strcasecmp(start, p->abbrev) == 0) {
            p->handler(tail);
            return;
        }
    }

    fprintf(stderr, "\"%s\": unknown command.\n", start);
}
