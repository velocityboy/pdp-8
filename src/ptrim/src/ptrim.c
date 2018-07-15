#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "pdp8/emulator.h"

static uint16_t rim[] = {
    06014,                      /* 7756, RFC */
    06011,                      /* 7757, LOOP, RSF */
    05357,                      /* JMP .-1 */
    06016,                      /* RFC RRB */
    07106,                      /* CLL RTL*/
    07006,                      /* RTL */
    07106,                      /* CLL RTL*/
    07006,                      /* RTL */
    07510,                      /* SPA*/
    05374,                      /* JMP 7774 */
    07006,                      /* RTL */
    06011,                      /* RSF */
    05367,                      /* JMP .-1 */
    06016,                      /* RFC RRB */
    07420,                      /* SNL */
    03776,                      /* DCA I 7776 */
    03376,                      /* 7774, DCA 7776 */
    05357,                      /* JMP 7757 */
    00000,                      /* 7776, 0 */
    05301                       /* 7777, JMP 7701 */
};

static uint16_t rimStart = 07756;
static int rimWords = sizeof(rim)/sizeof(rim[0]);

int main(int argc, char *argv[]) {
  uint16_t core[4096];

  memset(core, 0, sizeof(core));

  for (int i = 0; i < rimWords; i++) {
    core[rimStart+i] = rim[i];
  }

  if (argc != 2) {
    fprintf(stderr, "ptrim paper-tape\n\tSimulates RIM loader\n");
    return 1;
  }

  FILE *fp = fopen(argv[1], "rb");
  if (fp == NULL) {
    perror(argv[1]);
    return 2;
  }

  /* 7756 06014 RFC ; clear reader flag */

  /* for convenience we'll treat the 13th bit as the link register */
  uint16_t ac = 0;
  const uint16_t SIGN_BIT = 004000;
  const uint16_t LINK_BIT = 010000;

  for(;;) {
    /* 7757 06011 RSF ; skip when reader flag set */
    /* 7760 05357 JMP .-1 ; until byte read */
    int ch = fgetc(fp);
    if (ch == EOF) {
      break;
    }

    /* 7761 06016 RFC RRB ; read byte */
    ac |= (uint8_t)ch;

    /* 7762 07107 CLL RTL ; clear link and rotate left twice */
    ac <<= 2;

    /* 7763 07106 RTL ; rotate left twice */
    ac <<= 2;

    /* 7764 07510 SPA ; skip next intruction if ac >= 0 */
    /* 7765 06374 JMP 7774 ; JMP 7774 if AC < 0 */

    if (ac & SIGN_BIT) {
      goto L7774;
    }

    /* 7766 07106 RTL ; rotate left twice */
    ac <<= 2;

    /* 7767 06011 RSF ; skip when reader flag set */
    /* 7770 05367 JMP .-1 ; wait until reader ready */
    ch = fgetc(fp);
    if (ch == EOF) {
      break;
    }

    /* 7771 06016 RFC RRB ; read byte */
    ac |= (uint8_t)ch;

    /* 7772 07420 SNL ; skip if link set */
    if ((ac & LINK_BIT) == 0) {
      /* 7773 03776 DCA I 7776 ; store word and clear AC */
      int at = core[07776];
      core[at] = ac & 07777;
      ac = 0;
    }
L7774:
    /* 7774 03376 DCA 7776 ; store AC (ptr) in 7776 */
    core[07776] = ac & 07777;
    ac = 0;

    /* 7775 5357 JMP 7757 */
  }

  for (int addr = 0; addr <= 07777; addr++) {
    char line[200];
    /* we are assuming the boot code only uses EAE mode A */
    pdp8_disassemble((uint16_t)addr, &core[addr], 0, line, sizeof(line));
    printf("%s\n", line);
  }




  return 0;
}

#if 0
// paper tape RIM loader 
#endif
