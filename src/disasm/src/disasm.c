#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pdp8/emulator.h"

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

  /* overallocate in case the last instruction is incorrectly a 
   * truncate two-word op
   */
  uint16_t *wordBuffer = (uint16_t*)malloc(len + 2);
  size_t words = len / 2;

  for (size_t i = 0; i < words; i++) {
    wordBuffer[i] = (byteBuffer[2*i] << 6) | byteBuffer[2*i+1];
  }
  free(byteBuffer);

  for (size_t i = 0; i < words; i++) {
    char decoded[100];
    /* we are assuming EAE mode A */
    pdp8_disassemble(i, &wordBuffer[i], 0, decoded, sizeof(decoded));
    printf("%s\n", decoded);
  }

  return 0;
}


