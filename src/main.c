#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_builtin(const char *str);

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  while (1) {
    int command_found = 0;

    printf("$ ");
    
    char input[100];
    if (!fgets(input, 100, stdin))
      break;

    input[strcspn(input, "\n")] = '\0';
      
    if (!strcmp(input, "exit"))
      break;
    if (!strncmp(input, "echo", 4) && strlen(input) > 5) {
      command_found = 1;
      const char *rest = strchr(input, ' ') + 1;
      if (rest != NULL)
        puts(rest);
    }
    if (!strncmp(input, "type", 4) && strlen(input) > 5) {
      command_found = 1;
      const char *rest = strchr(input, ' ') + 1;
      if (rest != NULL) {
        if (is_builtin(rest))
          printf("%s is a shell builtin\n", rest);
        else
          printf("%s: not found\n", rest);
      }
    }
    if (!command_found)
      printf("%s: command not found\n", input);
  }

  return 0;
}

int is_builtin(const char *str) {
  const char *builtins[3] = {"exit", "echo", "type"};
  
  for (int i = 0; i < 3; ++i) {
    if (!strcmp(str, builtins[i]))
      return 1;
  }

  return 0;
}
