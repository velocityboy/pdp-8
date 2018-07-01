#include <stdarg.h>
#include <stdio.h>

#include "dislib.h"

typedef struct Buffer {
  char *next;
  int remainingLen;
  int overflow;
} Buffer;

typedef struct Iot {
  uint16_t opcode;
  char *mnemonic;
} Iot;

static Iot knownIots[] = {
  { 06010, "RPE" },
  { 06011, "RSF" },
  { 06012, "RRB" },
  { 06013, "RRB RSF" },
  { 06014, "RFC" },
  { 06015, "RFC RSF" },
  { 06016, "RRB RFC" },
  { 06017, "RFC RRB RSF "},

  // these are for the console TTY
  { 06030, "KCF" },
  { 06031, "KSF" },
  { 06032, "KCC" },
  { 06034, "KRS" },
  { 06035, "KIE" },
  { 06036, "KRB" },
  { 0, NULL },
};

static const int MEM_OPS = 6;
static const int IOT_OP = 6;
static const int OPR_OP = 7;
static char *memOps[MEM_OPS] = {
  "AND", "TAD", "ISZ", "DCA", "JMS", "JMP",
};

static const int OPCODE_SHIFT = 9;
static const int OPCODE_MASK = 07;

static void memoryOp(Buffer *buf, const char *opcode, uint16_t word, uint16_t pc);
static void iot(Buffer *buf, uint16_t word);
static void opr(Buffer *buf, uint16_t word);
static void appendf(Buffer *buffer, char *fmt, ...);

int pdp8Disassemble(uint16_t addr, uint16_t op, char *decoded, int decodedLen) {
  Buffer buf; 
  buf.next = decoded;
  buf.remainingLen = decodedLen;

  if (decodedLen == 0) {
    return -1;
  }

  appendf(&buf, "%05o %04o ", addr, op);

  int opcode = (op >> OPCODE_SHIFT) & OPCODE_MASK;
  if (opcode < MEM_OPS) {
    memoryOp(&buf, memOps[opcode], op, addr);
  } else if (opcode == IOT_OP) {
    iot(&buf, op);
  } else if (opcode == OPR_OP) {
    opr(&buf, op);
  }

  return buf.overflow ? -1 : 0;
}

static void memoryOp(Buffer *buf, const char *opcode, uint16_t word, uint16_t pc) {
  uint16_t pcRelative = word & 0x0080;  
  uint16_t indirect = word & 0x0100;
  uint16_t target = word & 0177;

  appendf(buf, "%s ", opcode);
  if (pcRelative) {
    target |= (pc & 07600);
  }
  if (indirect) {
    appendf(buf, "I ");
  }

  appendf(buf, "%04o", target);
}

static void iot(Buffer *buf, uint16_t word) {
  for (int i = 0; knownIots[i].mnemonic; i++) {
    if (knownIots[i].opcode == word) {
      appendf(buf, "%s", knownIots[i].mnemonic);
      return;
    }
  }

  appendf(buf, "device=%o,func=%o", (int)((word >> 3) & 077), (int)(word & 07));
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

static void oprGroup(Buffer *buf, uint16_t word, const opr_bit *bits) {
  for (const opr_bit *p = bits; p->bit != 0; p++) {
    if ((word & p->bit) != 0) {
      appendf(buf, "%s ", p->mnemonic);
    }
  }
}

static void oprGroup1(Buffer *buf, uint16_t word) {
  oprGroup(buf, word, opr_group1);

  if ((word & 0014) == 0) {
    if (word & 0002) {
      appendf(buf, "BSW ");
    }
  } else {
    int byTwo = word & 0002;
    if (word & 0004) {
      appendf(buf, byTwo ? "RLT " : "RAL ");
    }
    if (word & 0010) {
      appendf(buf, byTwo ? "RRT " : "RAR ");
    }
  }
}

static void opr(Buffer *buf, uint16_t word) {
  if ((word & 0400) == 0) {
    oprGroup1(buf, word);
  } else if ((word & 0411) == 0400) {
    oprGroup(buf, word, opr_group2_or);
  } else if ((word & 0411) == 0410) {
    oprGroup(buf, word, opr_group2_and);
  } else if ((word & 0401) == 0401) {
    oprGroup(buf, word, opr_group3);
    appendf(buf, "%s", opr_group3_code[(word >> 1) & 07]);
  }
}

static void appendf(Buffer *buffer, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(buffer->next, buffer->remainingLen, fmt, args);
  va_end(args);

  if (n > buffer->remainingLen) {
    n = buffer->remainingLen;
    buffer->overflow = 1;
  }

  buffer->next += n;  
  buffer->remainingLen -= n;
}

