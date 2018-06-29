#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define CORE_SIZE 4096 /* 12-bit words */

typedef struct CPU {
  uint16_t core[CORE_SIZE];
  uint16_t a;
  int l; /* link flag */
  uint16_t pc;
} CPU;

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

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "emu: paper-tape-file\n");
    return 1;
  }

  CPU cpu;
  if (readBootTapeImage(argv[1], cpu.core, CORE_SIZE) == -1) {
    return 1;
  }  
}

