#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  printf("$ ");

  // stage 2 assumption: everything produces an error, even valid commands
  // TODO: change this to use actual structured code that works correctly and not just works based on simple assumptions
  char buf[100]; /* temp variable where the command line goes for now, TODO: FIXIT */
  scanf("%99s", buf);

  printf("%s: command not found\n", buf);

  return 0;
}
