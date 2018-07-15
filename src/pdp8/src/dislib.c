#include <stdarg.h>
#include <stdio.h>

#include "pdp8/emulator.h"

typedef struct buffer_t {
  char *next;
  int remaining;
  int overflow;
} buffer_t;

typedef struct iot_t {
  uint16_t opcode;
  char *mnemonic;
} iot_t;

static iot_t known_iots[] = {
  { 06010, "RPE" },
  { 06011, "RSF" },
  { 06012, "RRB" },
  { 06013, "RRB RSF" },
  { 06014, "RFC" },
  { 06015, "RFC RSF" },
  { 06016, "RRB RFC" },
  { 06017, "RFC RRB RSF "},

  /* console TTY */
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
static char *memops[MEM_OPS] = {
  "AND", "TAD", "ISZ", "DCA", "JMS", "JMP",
};

static const int OPCODE_SHIFT = 9;
static const int OPCODE_MASK = 07;

static void memory_op(buffer_t *buf, const char *opcode, uint16_t word, uint16_t pc);
static void iot(buffer_t *buf, uint16_t word);
static int  opr(buffer_t *buf, uint16_t *word, int eae_mode_b);
static void appendf(buffer_t *buffer, char *fmt, ...);

int pdp8_disassemble(uint16_t addr, uint16_t *op, int eae_mode_b, char *decoded, int decoded_size) {
  buffer_t buf; 
  buf.next = decoded;
  buf.remaining = decoded_size;

  if (decoded_size <= 0) {
    return -1;
  }

  appendf(&buf, "%05o %04o ", addr, *op);

  int opcode = (*op >> OPCODE_SHIFT) & OPCODE_MASK;
  if (opcode < MEM_OPS) {
    memory_op(&buf, memops[opcode], *op, addr);
  } else if (opcode == IOT_OP) {
    iot(&buf, *op);
  } else if (opcode == OPR_OP) {
    /* opr can be two words */
    return opr(&buf, op, eae_mode_b);
  }

  return buf.overflow ? -1 : 1;
}

static void memory_op(buffer_t *buf, const char *opcode, uint16_t word, uint16_t pc) {
  uint16_t pc_rel = word & 0x0080;  
  uint16_t indirect = word & 0x0100;
  uint16_t target = word & 0177;

  appendf(buf, "%s ", opcode);
  if (pc_rel) {
    target |= (pc & 07600);
  }
  if (indirect) {
    appendf(buf, "I ");
  }

  appendf(buf, "%04o", target);
}

static void iot(buffer_t *buf, uint16_t word) {
  if ((word & 0700) == 0200) {
    /* memory extension control */
    int field = (word & 0070) >> 3;
    int func = word & 0007;

    static char *func4ops[] = {
      "CINT",
      "RDF",
      "RIF",
      "RIB",
      "RMF",
      "SINT",
      "CUF",
      "SUF",      
    };

    switch (func) {
      case 1:
        appendf(buf, "CDF %d", field);
        break;

      case 2:
        appendf(buf, "CIF %d", field);
        break;

      case 3:
        appendf(buf, "CDF CIF %d", field);
        break;

      case 4:
        appendf(buf, "%s", func4ops[field]);
        break;

      default:
        appendf(buf, "device=%o,func=%o", (int)((word >> 3) & 077), (int)(word & 07));
        break;
    }
    return;
  }

  for (int i = 0; known_iots[i].mnemonic; i++) {
    if (known_iots[i].opcode == word) {
      appendf(buf, "%s", known_iots[i].mnemonic);
      return;
    }
  }

  appendf(buf, "device=%o,func=%o", (int)((word >> 3) & 077), (int)(word & 07));
}

typedef struct opr_bit_t {
  uint16_t bit;
  const char *mnemonic;
} opr_bit_t;

static opr_bit_t opr_group1_bits[] = {
  { 0200, "CLA" },
  { 0100, "CLL" },
  { 0040, "CMA" },
  { 0020, "CML" },
  { 0001, "IAC" },
  { 0, NULL },
};

static opr_bit_t opr_group2_or[] = {
  { 0200, "CLA" },
  { 0100, "SMA" },
  { 0040, "SZA" },
  { 0020, "SNL" },
  { 0004, "OSR" },
  { 0002, "HLT" },
  { 0, NULL },
};

static opr_bit_t opr_group2_and[] = {
  { 0200, "CLA" },
  { 0100, "SPA" },
  { 0040, "SNA" },
  { 0020, "SZL" },
  { 0004, "OSR" },
  { 0002, "HLT" },
  { 0, NULL },
};

static opr_bit_t opr_group3_a[] = {
  { 0200, "CLA" },
  { 0100, "MQA" },
  { 0040, "SCA" },
  { 0020, "MQL" },
};

static opr_bit_t opr_group3_b[] = {
  { 0200, "CLA" },
  { 0100, "MQA" },
  { 0020, "MQL" },
};

static void opr_group(buffer_t *buf, uint16_t word, const opr_bit_t *bits) {
  for (const opr_bit_t *p = bits; p->bit != 0; p++) {
    if ((word & p->bit) != 0) {
      appendf(buf, "%s ", p->mnemonic);
    }
  }
}

static void opr_group1(buffer_t *buf, uint16_t word) {
  opr_group(buf, word, opr_group1_bits);

  if ((word & 0014) == 0) {
    if (word & 0002) {
      appendf(buf, "BSW ");
    }
  } else {
    int by_two = word & 0002;
    if (word & 0004) {
      appendf(buf, by_two ? "RLT " : "RAL ");
    }
    if (word & 0010) {
      appendf(buf, by_two ? "RRT " : "RAR ");
    }
  }
}

static int group3(buffer_t *buffer, uint16_t word[2], int eae_mode_b) {
  if (!eae_mode_b) {
    switch ((word[0] >> 1) & 07) {
      case 0:
        break;

      case 1:
        appendf(buffer, "SCL %02o", (~word[1]) & 037);
        return 2;

      case 2:
        appendf(buffer, "MUY %05o", word[1]);
        return 2;

      case 3:
        appendf(buffer, "DIV %05o", word[1]);
        return 2;

      case 4:
        appendf(buffer, "NMI");
        return 1;

      case 5:
        appendf(buffer, "SHL %02o", word[1] + 1);
        return 2;

      case 6:
        appendf(buffer, "ASR %02o", word[1] + 1);
        return 2;

      case 7:
        appendf(buffer, "LSR %02o", word[1] + 1);
        return 2;
    }

    return 1;
  }

  int code = ((word[0] >> 1) & 07) | ((word[0] >> 2) & 010);

  switch (code) {
    case 0:
      break;

    case 1:
      appendf(buffer, "ACS %02o", word[1] & 037);
      return 2;

    case 2:
      appendf(buffer, "MUY @%05o", word[1]);
      return 2;

    case 3:
      appendf(buffer, "DIV @%05o", word[1]);
      return 2;

    case 4:
      appendf(buffer, "NMI");
      return 1;

    case 5:
      appendf(buffer, "SHL %02o", word[1]);
      return 2;

    case 6:
      appendf(buffer, "ASR %02o", word[1]);
      return 2;

    case 7:
      appendf(buffer, "LSR %02o", word[1]);
      return 2;

    case 8:
      appendf(buffer, "SCA");
      return 1;

    case 9:
      appendf(buffer, "DAD %05o", word[1]);
      return 2;

    case 10:
      appendf(buffer, "DST %05o", word[1]);
      return 2;

    case 11:
      appendf(buffer, "SWBA");
      return 1;

    case 12:
      appendf(buffer, "DPSZ");
      return 1;

    case 13:
      appendf(buffer, "DPIC");
      return 1;

    case 14:
      appendf(buffer, "DCM");
      return 1;

    case 15:
      appendf(buffer, "SAM");
      return 1;
  }

  return 1;
}


static int opr(buffer_t *buf, uint16_t *pword, int eae_mode_b) {
  uint16_t word = *pword;
  if ((word & 0400) == 0) {
    opr_group1(buf, word);
  } else if ((word & 0411) == 0400) {
    opr_group(buf, word, opr_group2_or);
  } else if ((word & 0411) == 0410) {
    opr_group(buf, word, opr_group2_and);
  } else if ((word & 0401) == 0401) {
    opr_group(buf, word, eae_mode_b ? opr_group3_b : opr_group3_a);
    return group3(buf, pword, eae_mode_b);
  }

  return 1;
}

static void appendf(buffer_t *buffer, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(buffer->next, buffer->remaining, fmt, args);
  va_end(args);

  if (n > buffer->remaining) {
    n = buffer->remaining;
    buffer->overflow = 1;
  }

  buffer->next += n;  
  buffer->remaining -= n;
}

