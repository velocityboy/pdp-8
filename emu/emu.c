#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../disasm/dislib.h"

#define CORE_SIZE 4096 /* 12-bit words */

typedef struct CPU {
  uint16_t core[CORE_SIZE];
  uint16_t ac;
  int l; /* link flag */
  uint16_t pc;
} CPU;

static inline uint16_t mask12(uint16_t value) {
  return value & 07777;
}

static inline int is12Bit(uint16_t value) {
  return value <= 07777;
}

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

  while (1) {
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
    }
  }
}

