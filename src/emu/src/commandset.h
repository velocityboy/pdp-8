#ifndef _COMMANDSET_H_
#define _COMMANDSET_H_

typedef struct command_t command_t;
struct command_t {
    char *command;
    char *abbrev;
    char *help;
    void (*handler)(char *tail);
};

extern void execute_command(char *command, command_t *command_set);

#endif
