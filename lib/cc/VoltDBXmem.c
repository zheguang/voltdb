#include <libxmem.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void voltDBXmemInit() {
  fprintf(stderr, "[debug] voltdb xmem init\n");
  xmem_init();
}

void voltDBXmemDestroy() {
  fprintf(stderr, "[debug] voltdb xmem destroy\n");
  xmem_destroy();
}

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("usage: VoltDBXmem.out <mode>\n");
    exit(1);
  }

  const char* mode = argv[1];
  if (strcmp(mode, "init") == 0) {
    voltDBXmemInit();
  } else if (strcmp(mode, "destroy") == 0) {
    voltDBXmemDestroy();
  }

  return 0;
}
