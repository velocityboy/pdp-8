#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void memoryOp(const char *opcode, uint16_t word);
static void iot(uint16_t word);
static void opr(uint16_t word);

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "disasm: image-file\n");
    return 1;
  }

  FILE *fin = fopen(argv[1], "rb");
  if (!fin) {
    perror(argv[1]);
    return 1;
  }

  fseek(fin, 0L, SEEK_END);
  size_t len = ftell(fin);
  fseek(fin, 0L, SEEK_SET);

  uint8_t *byteBuffer = (uint8_t*)malloc(len);
  fread(byteBuffer, 1, len, fin);
  fclose(fin);

  uint16_t *wordBuffer = (uint16_t*)malloc(len);
  size_t words = len / 2;

  for (size_t i = 0; i < words; i++) {
    wordBuffer[i] = (byteBuffer[2*i+1] << 8) | byteBuffer[2*i];
  }
  free(byteBuffer);

  for (size_t i = 0; i < words; i++) {
    printf("%05o ", (int)i);
    uint16_t w = wordBuffer[i];
    printf("%04o ", w);
    switch ((w >> 9) & 0x07) {
    case 0:
      memoryOp("AND", w);
      break;

    case 1:
      memoryOp("TAD", w);
      break;

    case 2:
      memoryOp("ISZ", w);
      break;

    case 3:
      memoryOp("DCA", w);
      break;

    case 4:
      memoryOp("JMS", w);
      break;

    case 5:
      memoryOp("JMP", w);
      break;

    case 6:
      iot(w);
      break;

    case 7:
      opr(w);
      break;
      
    default:
      printf("???\n");
    }
  }

  

  return 0;
}

static void memoryOp(const char *opcode, uint16_t word) {
  uint16_t pcRelative = word & 0x0080;
  uint16_t indirect = word & 0x0100;

  printf("%s ", opcode);
  if (pcRelative) {
    printf("PC ");
  }
  if (indirect) {
    printf("I ");
  }

  printf("%03o\n", word & 0177);
}

static void iot(uint16_t word) {
  printf("device=%o,func=%o\n", (int)((word >> 3) & 077), (int)(word & 07));
}

typedef struct opr_bit {
  uint16_t bit;
  const char *mnemonic;
} opr_bit;

static opr_bit opr_group1[] = {
  { 0200, "CLA" },
  { 0100, "CLL" },
  { 0040, "CMA" },
  { 0020, "CML" },
  { 0010, "RAR" },
  { 0004, "RAL" },
  { 0002, "BSW" },
  { 0001, "IAC" },
  { 0, NULL },
};

static opr_bit opr_group2_or[] = {
  { 0200, "CLA" },
  { 0100, "SMA" },
  { 0040, "SZA" },
  { 0020, "SNL" },
  { 0004, "OSR" },
  { 0002, "HLT" },
  { 0, NULL },
};

static opr_bit opr_group2_and[] = {
  { 0200, "CLA" },
  { 0100, "SPA" },
  { 0040, "SNA" },
  { 0020, "SZL" },
  { 0004, "OSR" },
  { 0002, "HLT" },
  { 0, NULL },
};

static opr_bit opr_group3[] = {
  { 0200, "CLA" },
  { 0100, "MQA" },
  { 0040, "SCA" },
  { 0020, "MQL" },
};

static const char *opr_group3_code[] = {
  "",
  "SCL",
  "MUY",
  "DIV",
  "NMI",
  "SHL",
  "ASR",
  "LSR",
};

static void oprGroup(uint16_t word, const opr_bit *bits) {
  for (const opr_bit *p = bits; p->bit != 0; p++) {
    if ((word & p->bit) != 0) {
      printf("%s ", p->mnemonic);
    }
  }
}

static void opr(uint16_t word) {
  printf("OPR ");
  if ((word & 0400) == 0) {
    oprGroup(word, opr_group1);
  } else if ((word & 0411) == 0400) {
    oprGroup(word, opr_group2_or);
  } else if ((word & 0411) == 0410) {
    oprGroup(word, opr_group2_and);
  } else if ((word & 0401) == 0401) {
    oprGroup(word, opr_group3);
    printf("%s", opr_group3_code[(word >> 1) & 07]);
  }

  putchar('\n');
}
