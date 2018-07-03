#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pdp8/dislib.h"

#define CORE_SIZE 4096 /* 12-bit words */

typedef struct CPU {
  uint16_t core[CORE_SIZE];
  uint16_t ac;
  int l; /* link flag */
  uint16_t pc;
  uint16_t sr;    /* front panel switches */
  int run;
} CPU;

static inline uint16_t mask12(uint16_t value) {
  return value & 07777;
}

static inline int is12Bit(uint16_t value) {
  return value <= 07777;
}

static inline int isGroup1(uint16_t opr) {
  return (opr & 0400) == 0;
}

static inline int isGroup2(uint16_t opr) {
  return (opr & 0401) == 0400;
}

static const uint16_t OPR_GRP1_CLA  = 0200;
static const uint16_t OPR_GRP1_CLL  = 0100;
static const uint16_t OPR_GRP1_CMA  = 0040;
static const uint16_t OPR_GRP1_CML  = 0020;
static const uint16_t OPR_GRP1_RAR  = 0010;
static const uint16_t OPR_GRP1_RAL  = 0004;
static const uint16_t OPR_GRP1_ROT2 = 0002;
static const uint16_t OPR_GRP1_IAC  = 0001;

static const uint16_t OPR_GRP2_CLA  = 0200;
static const uint16_t OPR_GRP2_SMA  = 0100;
static const uint16_t OPR_GRP2_SZA  = 0040;
static const uint16_t OPR_GRP2_SNL  = 0020;
static const uint16_t OPR_GRP2_INV  = 0010;
static const uint16_t OPR_GRP2_OSR  = 0004;
static const uint16_t OPR_GRP2_HLT  = 0002;

static int readBootTapeImage(char *filename, uint16_t *core, int coreWords) {
  int success = -1;
  uint8_t *image = NULL;
  FILE *fp = fopen(filename, "rb");

  if (fp == NULL) {
    perror(filename);
    goto done;
  }

  fseek(fp, 0L, SEEK_END);
  long imageBytes = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  if (imageBytes > coreWords * sizeof(uint16_t)) {
    printf("warning - paper tape image %s is longer than memory and will be truncated.\n", filename);
    imageBytes = coreWords * sizeof(uint16_t);
  }

  image = malloc(imageBytes);
  if (image == NULL) {
    fprintf(stderr, "out of memory\n");
    goto done;
  }

  if (fread(image, 1, imageBytes, fp) != imageBytes) {
    fprintf(stderr, "failed to read %s\n", filename);
    goto done;
  }

  int iw = 0;
  for (int ib = 0; ib < imageBytes; ib += 2) {
    core[iw++] = (image[ib + 1] << 8) | image[ib];
  }
  
  success = 0;

done:
  if (fp != NULL) {
    fclose(fp);
  }
  free(image);
  return success;
}

static uint16_t effectiveAddress(uint16_t op, CPU *cpu) {
  // low 7 bits come from op encoding
  uint16_t addr = op & 0177;

  // bit 4, if set, means in same page as PC
  if ((op & 0200) != 0) {
    uint16_t page = cpu->pc & 07600;
    addr |= page;
  }

  // bit 3, if set, means indirect
  if ((op & 0400) != 0) {
    addr = cpu->core[addr];
  }

  return addr;
}

static void opr(uint16_t op, CPU *cpu) {
  if (isGroup1(op)) {
    /* CLA, CLL, CMA, CLL are all in event slot 1, but STL is CLL + CML, so
     * the clears must apply first.
     */
    if ((op & OPR_GRP1_CLA) != 0) { cpu->ac = 0; }
    if ((op & OPR_GRP1_CLL) != 0) { cpu->l  = 0; }

    if ((op & OPR_GRP1_CMA) != 0) { cpu->ac ^= 07777; }
    if ((op & OPR_GRP1_CML) != 0) { cpu->l ^= 01; }

    /* NOTE model specific - on the 8/I and later, IAC happens before the shifts; in 
     * earlier models, they happened at the same time.
     */
    if ((op & OPR_GRP1_IAC) != 0) { cpu->ac = mask12(cpu->ac + 1); }

    uint16_t shift = op & (OPR_GRP1_RAL | OPR_GRP1_RAR);

    int by = ((op & OPR_GRP1_ROT2) != 0) ? 2 : 1;
    if (shift == 0) {
      /* model specific - on 8/E and later, the double-shift bit by itself
       * swaps the low and high 6 bits of AC
       */       
      if ((op & OPR_GRP1_ROT2) != 0) {
        cpu->ac =
          ((cpu->ac & 077600) >> 6) | ((cpu->ac & 000177) << 6);
      }
    } else if (shift == OPR_GRP1_RAL) {
      while (by--) {
        cpu->ac <<= 1;
        cpu->ac |= cpu->l;
        cpu->l = (cpu->ac >> 12) & 01;
        cpu->ac = mask12(cpu->ac);
      }
    } else if (shift == OPR_GRP1_RAR) {
      while (by--) {
        uint16_t l = cpu->ac & 01;        
        cpu->ac >>= 1;
        cpu->ac |= (cpu->l << 11);
        cpu->l = l;
      }
    }

    return;
  }

  if (isGroup2(op)) {
    uint16_t invert = op & OPR_GRP2_INV;

    int skip = 0;

    if ((op & OPR_GRP2_SMA) != 0) {
      skip |= (invert ? cpu->ac >= 0 : cpu->ac < 0);
    }

    if ((op & OPR_GRP2_SZA) != 0) {
      skip |= (invert ? cpu->ac != 0 : cpu->ac == 0);
    }

    if ((op & OPR_GRP2_SNL) != 0) {
      skip |= (invert ? cpu->l == 0 : cpu->l != 0);
    }

    if (skip) {
      cpu->pc = mask12(cpu->pc + 1);
    }

    /* important: CLA happens *after* tests. */
    if ((op & OPR_GRP2_CLA) != 0) {
      cpu->ac = 0;      
    }

    if ((op & OPR_GRP2_HLT) != 0) {
      cpu->run = 0;
    }

    if ((op & OPR_GRP2_OSR) != 0) {
      cpu->ac |= cpu->sr;
    }



    return;
  }


}


int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "emu: paper-tape-file\n");
    return 1;
  }

  CPU cpu;
  memset(&cpu, 0, sizeof(cpu));

  if (readBootTapeImage(argv[1], cpu.core, CORE_SIZE) == -1) {
    return 1;
  }

  cpu.run = 1;

  while (cpu.run) {
    uint16_t op = cpu.core[cpu.pc];
    uint16_t ea;
    uint16_t sum;
    cpu.pc = mask12(cpu.pc + 1);

    switch ((op >> 9) & 07) {
      case 0: /* AND */
        ea = effectiveAddress(op, &cpu);
        cpu.ac &= cpu.core[ea];
        break;

      case 1: /* TAD */
        ea = effectiveAddress(op, &cpu);
        sum = cpu.core[ea] + cpu.ac;
        cpu.ac = mask12(sum);
        if (sum & 010000) {
          cpu.l ^= 1;
        }
        break;

      case 2: /* ISZ */
        ea = effectiveAddress(op, &cpu);
        cpu.core[ea] = mask12(cpu.core[ea]);
        if (cpu.core[ea] == 0) {
          cpu.pc = mask12(cpu.pc + 1);
        }
        break;

      case 3: /* DCA */
        ea = effectiveAddress(op, &cpu);
        cpu.core[ea] = cpu.ac;
        cpu.ac = 0;
        break;

      case 4: /* JMS */
        ea = effectiveAddress(op, &cpu);
        cpu.core[ea] = cpu.pc;
        cpu.pc = mask12(ea + 1);
        break;

      case 5: /* JMP */
        cpu.pc = effectiveAddress(op, &cpu);
        break;

      case 6: /* IOT - io devices */
        break;

      case 7:
        opr(op, &cpu);
        break;
    }
  }
}

