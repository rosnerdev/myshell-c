#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  while (1) {
    printf("$ ");
    // stage 2/3 assumption: everything produces an error, even valid commands
    // TODO: change this to use actual structured code that works correctly and not just works based on simple assumptions
    char input[100];
    if (!fgets(input, 100, stdin))
      break;

    if (!strncmp(input, "exit", 4))
      break;
      
    input[strlen(input) - 1] = '\0';
    printf("%s: command not found\n", input);
  }

  return 0;
}
