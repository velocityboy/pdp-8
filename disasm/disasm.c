#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dislib.h"

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
    char decoded[100];
    pdp8Disassemble(i, wordBuffer[i], decoded, sizeof(decoded));
    printf("%s\n", decoded);
  }

  return 0;
}


