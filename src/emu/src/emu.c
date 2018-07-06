#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pdp8/emulator.h"
#include "commandset.h"
#include "options.h"

static inline int min(int x, int y) { return x < y ? x : y; }
static inline int isodigit(char ch) { return ch >= '0' && ch <= '7'; }

static void commandloop(FILE *fp);

static void unassemble(char *);
static void registers(char *);
static void set(char *);
static void step(char *);
static void deposit(char *);
static void examine(char *);
static void exit_(char *);
static void help(char *);
static void enable(char*);

static int octal(char **);
static char *trim(char *str);
    

static command_t commands[] = {
    { "deposit",    "d",  "xxxx dd [dd ...]", &deposit },
    { "enable",     "en", "option-name (lists options with no args)", &enable},
    { "examine",    "ex", "xxxxx[-yyyyy] examine core", &examine},
    { "exit",       "q",  "exit the emulator", &exit_},
    { "help",       "h",  "display help for all commands", &help},
    { "registers",  "r",  "display the CPU state", &registers},
    { "set",        "s",  "AC|PC|LINK|RUN|SR %o set register", &set},
    { "step",       "n",  "single step the CPU", &step},
    { "unassemble", "u",  "xxxxx[-yyyyy] unassemble memory", &unassemble},
    { NULL, NULL, NULL, NULL},
};

static pdp8_t *pdp8;

int main(int argc, char *argv[]) {
    pdp8 = pdp8_create();
    commandloop(stdin);
}

static void commandloop(FILE *fp) {
    for(;;) {
        char command[256];
        if (fp == stdin) {
            printf("pdp8> ");
        }
        if (fgets(command, sizeof(command), fp) == NULL) {
            break;
        }
        char *trimmed = trim(command);
        if (fp != stdin) {
            printf("-%s\n", trimmed);
        }

        if (trimmed[0] == '@') {
            FILE *nested = fopen(trimmed+1, "r");
            if (nested == NULL) {
                printf("could not open script \"%s\"\n", trimmed+1);
                continue;
            }
            commandloop(nested);
            fclose(nested);
            continue;
        }

        execute_command(trimmed, commands);
    }
}

static void exit_(char *tail) {
    exit(1);
}

static void help(char *tail) {
    for (command_t *p = commands; p->command; p++) {
        printf("%s", p->command);
        if (p->abbrev) {
            printf(" [%s]", p->abbrev);
        }
        printf(" %s\n", p->help);
    }

    printf("\n@filename executes lines of \"filename\" until end of file\n");
}

static void examine(char *tail) {
    int start = 0;
    int end = 0;
    int consumed = 0;
    int n = sscanf(tail, "%o-%o %n", &start, &end, &consumed);
    if (n <= 0) {
        printf("examine: invalid parameters\n");
        return;
    }

    if (n == 1) {
        end = start;
    }

    if (n == 2 && consumed != strlen(tail)) {
        printf("examine: invalid parameters\n");
        return;
    }

    if (end < start) {
        printf("end must not be less than start\n");
        return;
    }

    if (start >= pdp8->core_size) {
        return;
    }

    if (end >= pdp8->core_size) {
        end = pdp8->core_size - 1;
    }

    int len = end + 1 - start;
    const int words_per_line = 8;

    for (int offset = 0; offset < len; offset +=  words_per_line) {
        printf("%05o  ", start + offset);

        int line = min(len - offset, words_per_line);
        for (int i = 0; i < line; i++) {
            printf("%04o ", pdp8->core[start + offset + i]);
        }
        putchar('\n');
    }
}

static void unassemble(char *tail) {
    int start = 0;
    int end = 0;
    int consumed = 0;
    int n = sscanf(tail, "%o-%o %n", &start, &end, &consumed);
    if (n <= 0) {
        printf("unassemnble: invalid parameters\n");
        return;
    }

    if (n == 1) {
        end = start;
    }

    if (n == 2 && consumed != strlen(tail)) {
        printf("unassemble: invalid parameters\n");
        return;
    }


    if (end < start) {
        printf("end must not be less than start\n");
        return;
    }

    if (start >= pdp8->core_size) {
        return;
    }

    if (end >= pdp8->core_size) {
        end = pdp8->core_size - 1;
    }

    for (; start <= end; start++) {
        char line[200];
        pdp8_disassemble(start, pdp8->core[start], line, sizeof(line));
        printf("%s\n", line);
    }
}

static void registers(char *tail) {
    printf("AC %04o PC %04o SR %04o LINK %o", pdp8->ac, pdp8->pc, pdp8->sr, pdp8->link);
    if (pdp8->option_eae) {
        printf(" MQ %04o SC %04o", pdp8->mq, pdp8->sc);
    }
    printf(" %s\n", pdp8->run ? "RUN" : "STOP");
    char line[200];
    pdp8_disassemble(pdp8->pc, pdp8->core[pdp8->pc], line, sizeof(line));
    printf("%s\n", line);
}

static void step(char *tail) {
    pdp8_step(pdp8);
}

static void set(char *tail) {
    char regname[20];
    int value;
    int consumed;
    if (sscanf(tail, "%20s %o %n", regname, &value, &consumed) != 2 || consumed != strlen(tail)) {
        printf("set: invalid parameters\n");
    }

    pdp8_reg_t reg = pdp8_reg_from_string(regname);
    if (reg < 0) {
        printf("%s is not a value register\n", regname);
        return;
    }

    if ((reg == REG_LINK || reg == REG_RUN) && value != 0 && value != 1) {
        printf("possible values for %s are 0 or 1\n", regname);
        return;
    }

    if ((reg == REG_MQ || reg == REG_SC) && !pdp8->option_eae) {
        printf("%s is only available if the EAE option is installed\n", regname);
        return;
    }

    if (value < 0 || value > 07777) {
        printf("values for %s must be in the range [0000..7777]\n", regname);
        return;
    }

    switch (reg) {
        case REG_AC: pdp8->ac = value; break;
        case REG_PC: pdp8->pc = value; break;
        case REG_LINK: pdp8->link = value; break;
        case REG_RUN: pdp8->run = value; break;
        case REG_SR: pdp8->sr = value; break;
        case REG_MQ: pdp8->mq = value; break;
        case REG_SC: pdp8->sc = value; break;
        default: break;
    }
}

static void deposit(char *tail) {
    if (!isodigit(*tail)) {
        printf("deposit: invalid parameters\n");
        return;
    }

    int addr = octal(&tail);

    while (1) {
        while (isspace(*tail)) {
            tail++;
        }

        if (!*tail) {
            break;
        }

        if (!isodigit(*tail)) {
            printf("data must be octal\n");
            return;
        }

        if (addr >= pdp8->core_size) {
            printf("%o is larger than core; cannot store.\n", addr);
            return;
        }

        int data = octal(&tail);
        if (data > 07777) {
            printf("data is greater than 7777; cannot store.\n");
            return;
        }

        pdp8->core[addr++] = data;
    }
}

static void enable(char *name) {
    if (*name == '\0') {
        printf("available options:\n\t");
        char **options = get_option_names();
        for (; *options; options++) {
            printf("%s ", *options);
        }
        printf("\n");
        return;
    }

    if (enable_option(name, pdp8) == -1) {
        printf("failed to enable option \"%s\"\n", name);
    }
}

static int octal(char **str) {
    int out = 0;

    while (isodigit(**str)) {
        out = out * 8 + (**str - '0');
        (*str)++;
    }

    return out;
}

static char *trim(char *str) {
    char *p = str;
    while (isspace(*p)) {
        p++;
    }

    if (!*p) {
        return p;
    }

    char *q = p + strlen(p);
    while (isspace(q[-1])) {
        q--;
    }

    *q = '\0';
    return p;
}
