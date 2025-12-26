#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  while (1) {
    printf("$ ");
    
    char input[100];
    if (!fgets(input, 100, stdin))
      break;

    input[strcspn(input, "\n")] = '\0';
      
    if (!strcmp(input, "exit"))
      break;
    if (!strncmp(input, "echo", 4) && strlen(input) > 5) {
      const char *rest = strchr(input, ' ') + 1;
      if (rest != NULL)
        puts(rest);

      continue;
    }

    printf("%s: command not found\n", input);
  }

  return 0;
}
