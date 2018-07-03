#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pdp8/dislib.h"

typedef struct {
  long size;
  uint8_t *bytes;
} Prom;

static const uint16_t OP_LOADADDR = 0x8000;
static const uint16_t OP_LOADEX = 0x4000;
static const uint16_t OP_DEPOSIT = 0x2000;
static const uint16_t OP_START = 0x1000;


int readProm(char *fn, Prom *prom) {
  FILE *fp = fopen(fn, "rb");
  if (fp == NULL) {
    perror(fn);
    return -1;
  }

  fseek(fp, 0L, SEEK_END);
  prom->size = (int)ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  prom->bytes = malloc(prom->size);
  if (!prom->bytes) {
    fprintf(stderr, "Out of memory\n");
    return -1;
  }

  fread(prom->bytes, 1, prom->size, fp);
  fclose(fp);
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "dumpair rom-a rom-b\n\tDump boot PROM pair\n");
    return 1;
  }

  Prom prom[2];

  if (readProm(argv[1], &prom[0]) == -1 || readProm(argv[2], &prom[1]) == -1) {
    return 2;
  }

  if (prom[0].size != prom[1].size) {
    fprintf(stderr, "PROM files are not the same size\n");
    return 2;
  }

  uint16_t addr = 0;

  for (int i = 0; i+1 < prom[0].size; i += 2) {
    // PROM's are 4 bit and reads are interleaved, we need 4 reads
    // to get a 16-bit control word
    uint16_t word = 
      (prom[0].bytes[i] << 12) |
      (prom[1].bytes[i] << 8) |
      (prom[0].bytes[i+1] << 4) |
      (prom[1].bytes[i+1]);

    printf("%04o ", i);

    printf((word & OP_LOADADDR) ? "LA" : "  ");
    printf((word & OP_LOADEX) ? "LX" : "  ");
    printf((word & OP_DEPOSIT) ? "DE" : "  ");
    printf((word & OP_START) ? "ST" : "  ");


    printf(" : %04o", word & 07777);

    if (word & OP_LOADADDR) {
      addr = word & 07777;
    }

    if (word & OP_DEPOSIT) {
      char disasm[100];
      pdp8Disassemble(addr, word & 07777, disasm, sizeof(disasm));
      printf(" | %s", disasm);
      addr++;
    }

    printf("\n");
  }
}
