#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pdp8/emulator.h"
#include "commandset.h"

static inline int min(int x, int y) { return x < y ? x : y; }
static inline int isodigit(char ch) { return ch >= '0' && ch <= '7'; }

static int octal(char **);
static void unassemble(char *);
static void registers(char *);
static void set(char *);
static void step(char *);
static void deposit(char *);
static void examine(char *);
static void exit_(char *);
static void help(char *);

static command_t commands[] = {
    { "deposit",    "d",  "xxxx dd [dd ...]", &deposit },
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
    for(;;) {
        char command[256];
        printf("pdp8> ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        execute_command(command, commands);
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
}

static void examine(char *tail) {
    while (isspace(*tail)) {
        tail++;
    }

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
    while (isspace(*tail)) {
        tail++;
    }

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
    printf("AC %04o PC %04o SR %04o LINK %o %s\n", pdp8->ac, pdp8->pc, pdp8->sr, pdp8->link, pdp8->run ? "RUN" : "STOP");
    char line[200];
    pdp8_disassemble(pdp8->pc, pdp8->core[pdp8->pc], line, sizeof(line));
    printf("%s\n", line);
}

static void step(char *tail) {
    pdp8_step(pdp8);
}

static void set(char *tail) {
    while (isspace(*tail)) {
        tail++;
    }

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
        default: break;
    }
}

static void deposit(char *tail) {
    while (isspace(*tail)) {
        tail++;
    }
    
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

static int octal(char **str) {
    int out = 0;

    while (isodigit(**str)) {
        out = out * 8 + (**str - '0');
        (*str)++;
    }

    return out;
}
