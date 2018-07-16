#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>

#include "pdp8/emulator.h"
#include "pdp8/devices.h"
#include "commandset.h"
#include "emu.h"
#include "options.h"
#include "pt_driver.h"
#include "tty_driver.h"
#include "bootprom.h"

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
static void go(char *);
static void bootprom(char *);
static void devlist(char *);
static void device(char *);
static void media(char *);
static void trace(char *);

static int dev_by_name(char *p, int *slot);    
static int octal(char **);
static char *trim(char *str);
static int parse(char *str, char **tokens, int max_tok);
    
static command_t commands[] = {
    { "bootprom",   "bp", "xxx load bootstrap from prom address xxx", &bootprom },
    { "deposit",    "d",  "xxxx dd [dd ...]", &deposit },
    { "dev",        "de", "device settings value", &device },
    { "devlist",    "dl", "show installed devices", &devlist },
    { "enable",     "en", "option-name (lists options with no args)", &enable },
    { "examine",    "ex", "xxxxx[-yyyyy] examine core", &examine },
    { "exit",       "q",  "exit the emulator", &exit_ },
    { "go",         "g",  "start execution", &go },
    { "help",       "h",  "display help for all commands", &help },
    { "media",      "m",  "load|unload device [fname]", &media },
    { "registers",  "r",  "display the CPU state", &registers },
    { "set",        "s",  "AC|PC|LINK|RUN|SR %o set register", &set },
    { "step",       "n",  "single step the CPU", &step },
    { "trace",      "t",  "start filename|stop|list trace-file list-file", &trace },
    { "unassemble", "u",  "xxxxx[-yyyyy] unassemble memory", &unassemble },
    { NULL, NULL, NULL, NULL},
};

static pdp8_t *pdp8;
static tty_driver_t *tty;

static const int DEVICE_SERVICE_INTERVAL = 256;
static void device_service(void *);

static const int MAX_DEVICES = 32;
static emu_device_t devices[MAX_DEVICES];

int main(int argc, char *argv[]) {
    pdp8 = pdp8_create();
    tty = emu_install_tty(pdp8);
    if (tty == NULL) {
        fprintf(stderr, "could not create tty driver\n");
        return 1;
    }

    if (emu_install_pt(pdp8) < 0) {
        fprintf(stderr, "could not create pt driver\n");
        return 1;
    }

    commandloop(stdin);
}

int emu_register_device(emu_device_t *device) {
    if (device->name == NULL) {
        return PDP8_ERR_INVALID_ARG;
    }

    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].name == NULL) {
            devices[i] = *device;
            return 0;
        }
    }

    return PDP8_ERR_MEMORY;
}

void emu_unregister_device(int dev_id) {
    if (dev_id < 0 || dev_id >= MAX_DEVICES || devices[dev_id].name == NULL) {
        return;
    }

    memset(&devices[dev_id], 0, sizeof(devices[dev_id]));
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

    while (1) {
        char line[200];
        int n = pdp8_disassemble(start, &pdp8->core[start], pdp8->eae_mode_b, line, sizeof(line));
        if (n <= 0) {
            break;            
        }        
        start = (start + n) & 07777;
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
    pdp8_disassemble(pdp8->pc, &pdp8->core[pdp8->pc], pdp8->eae_mode_b, line, sizeof(line));
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

static char *halt_reason(pdp8_halt_reason_t halt) {
    switch (halt) {
        case PDP8_HALT_HLT_INSTRUCTION:
            return "executed HLT instruction";

        case PDP8_HALT_SWP_UNSUPPORTED:
            return "SWP unsupported on current CPU";

        case PDP8_HALT_IAC_ROTS_UNSUPPORTED:
            return "IAC with rotations not supported on current CPU";

        case PDP8_HALT_CMA_ROTS_UNSUPPORTED:
            return "CMA with rotations not supported on current CPU";

        case PDP8_HALT_CLA_NMI_UNSUPPORTED:
            return "CLA with NMI not supported on current CPU";

        case PDP8_HALT_SCL_UNSUPPORTED:
            return "SCL unsupported on current CPU";

        case PDP8_HALT_CAF_HANG:
            return "CAF unsupported on current CPU";
            
        case PDP8_HALT_FRONT_PANEL:
            return "requested by operator";

        case PDP8_HALT_DEVICE_REQUEST:
            return "requested by device";

        default:
            return "unknown reason";
    }
}

static void go(char *args) {
    const int BLOCK = 100;
    const int BLOCK_USEC = 120; /* fastest instruction is 1.2 usec */
    const int USEC_PER_SEC = 1000 * 1000;

    emu_start_tty(tty);
    pdp8->run = 1;
    while (pdp8->run) {
        struct timeval start;
        gettimeofday(&start, NULL);

        for (int i = 0; i < BLOCK; i++) {
            pdp8_step(pdp8);
        }
        struct timeval end;
        gettimeofday(&end, NULL);

        if (end.tv_usec < start.tv_usec) {
            end.tv_usec += USEC_PER_SEC;
            end.tv_sec--;
        }

        end.tv_sec -= start.tv_sec;
        end.tv_usec -= start.tv_usec;

        if (end.tv_sec > 0 || end.tv_usec >= BLOCK_USEC) {
            continue;
        }

        end.tv_usec = BLOCK_USEC - end.tv_usec;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);

        /* wait for block execution time or until stdin ready */
        select(1, &fds, NULL, NULL, &end);
    }
    emu_end_tty(tty);

    if (!pdp8->run) {
        printf("\nhalt: %s\n", halt_reason(pdp8->halt_reason));
    }
}

static void bootprom(char *tail) {
    int addr = 0;
    int consumed = 0;

    int n = sscanf(tail, "%o %n", &addr, &consumed);
    if (n != 1 || consumed != strlen(tail)) {
        printf("bootprom: invalid parameters\n");
        return;
    }

    emu_run_bootprom(pdp8, addr);
}

static void devlist(char *tail) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].name == NULL) {
            continue;
        }

        emu_device_t *dev = &devices[i];

        printf("%s", dev->name);
        if (dev->slots > 1) {
            printf("0-%d", dev->slots - 1);
        }

        if (dev->description) {
            printf(": %s", dev->description);
        }

        printf("\n");
        if (dev->load_media && dev->unload_media) {
            printf("\tcan load and unload media.\n");
        }

        if (dev->get_setting_names && dev->set_setting && dev->get_setting) {
            char **settings = (dev->get_setting_names)(dev->ctx);
            printf("\tsettings\n");
            for (char **p = settings; *p; p++) {
                int val = 0;
                (dev->get_setting)(dev->ctx, *p, &val);
                printf("\t\t%s %d\n", *p, val);
            }
        }
    }
}

static void device(char *tail) {
    char *tokens[4];
    int n = parse(tail, tokens, 4);
    if (n < 3) {
        printf("dev: device setting value\n");
        return;
    }

    int slot= 0;
    int dev = dev_by_name(tokens[0], &slot);
    if (dev < 0) {
        printf("dev: no device named %s\n", tokens[0]);
        return;
    }

    if (devices[dev].set_setting == NULL) {
        printf("dev: device %s has no settings.\n", tokens[0]);
        return;
    }

    int val;
    int used;
    if (sscanf(tokens[2], "%d %n", &val, &used) != 1 || used != strlen(tokens[2])) {
        printf("dev: invalid integer value %s\n", tokens[2]);
        return;
    }

    if (devices[dev].set_setting(devices[dev].ctx, tokens[1], val) < 0) {
        printf("dev: %s has no setting %s\n", tokens[0], tokens[1]);
    }
}


static int dev_by_name(char *p, int *slot) {
    char *q = p;
    while (*q && !isdigit(*q)) {
        q++;
    }

    *slot = 0;
    int len = strlen(p);

    if (q) {
        sscanf(q, "%d", slot);
        len = q - p;
    }

    int dev = 0;
    for (; dev < MAX_DEVICES; dev++) {
        if (devices[dev].name && strncasecmp(devices[dev].name, p, len) == 0) {
            break;
        }
    }

    if (dev == MAX_DEVICES) {
        return -1;
    }

    if (*slot >= devices[dev].slots) {
        return -1;
    }

    return dev;
}

static void media(char *tail) {
    char *tokens[4];
    int n = parse(tail, tokens, 4);
    if (n < 2) {
        printf("media: invalid arguments\n");
        return;
    }

    int load = 0;
    if (strcmp(tokens[0], "load") == 0) {
        load = 1;
    } else if (strcmp(tokens[0], "unload") != 0) {
        printf("media: must give 'load' or 'unload'\n");
        return;
    }

    if (load && n < 3) {
        printf("media: must give mount filename with 'load'\n");
        return;
    }

    int slot;
    int dev = dev_by_name(tokens[1], &slot);
    if (dev < 0) {
        printf("media: device %s does not exist.\n", tokens[1]);
        return;
    }

    if (devices[dev].load_media == NULL || devices[dev].unload_media == NULL) {
        printf("media: device %s cannot load or unload media\n", tokens[1]);
        return;
    }

    if (load) {
        devices[dev].load_media(devices[dev].ctx, slot, tokens[2]);        
    } else {
        devices[dev].unload_media(devices[dev].ctx, slot);
    }
}

static void trace(char *tail) {
    char *tokens[4];
    int n = parse(tail, tokens, 4);
    if (n < 1) {
        printf("trace: invalid arguments\n");
        return;
    }

    if (strcmp(tokens[0], "start") == 0) {
        if (n < 2) {
            printf("trace: start requires filename\n");
            return;
        }

        if (pdp8_start_tracing(pdp8, tokens[1]) < 0) {
            printf("trace: already tracing.\n");
            return;
        }
    } else if (strcmp(tokens[0], "stop") == 0) {
        int ret = pdp8_stop_tracing(pdp8);
        switch (ret) {
            case PDP8_ERR_INVALID_ARG:
                printf("trace: not tracing.\n");
                break;

            case PDP8_ERR_FILEIO:
                printf("trace: could not write trace file.\n");
                break;

            case 0:
                break;

            default:
                printf("trace: generic failure.\n");
        }
    } else if (strcmp(tokens[0], "list") == 0) {
        if (n < 3) {
            printf("trace: list requires trace-file list-file.\n");
            return;
        }
        if (pdp8_make_trace_listing(pdp8, tokens[1], tokens[2]) < 0) {
            printf("trace: failed to create listing.\n");
        }
    } else {
        printf("trace: invalid subcommand %s\n", tokens[0]);
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

static int parse(char *str, char **tokens, int max_tok) {
    int n = 0;
    while (*str && n < max_tok - 1) {
        while (isspace(*str))
            str++;

        if (!*str) {
            break;
        }

        if (*str == '"') {
            str++;
            if (!*str) {
                break;
            }
            tokens[n++] = str;

            while (*str && *str != '"') 
                str++;

            if (!*str) {
                break;
            }

            *str++ = '\0';
            continue;
        }

        tokens[n++] = str;
        while (*str && !isspace(*str))
            str++;
        if (*str) 
            *str++ = '\0';
    }

    tokens[n] = NULL;
    return n;
}

