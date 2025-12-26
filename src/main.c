#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

typedef struct {
  char path[256];
  int exists;
} file;

int is_builtin(const char *str);
file search_file_in_directory(const char *dir_path, const char *target_filename);

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
        if (is_builtin(rest)) {
          printf("%s is a shell builtin\n", rest);
        } else {
          char *path_env = getenv("PATH");
          char *path = strdup(path_env);

          if (path_env != NULL) {
            int exists = 0;
            char *tok = strtok(path, ":");
            while (tok != NULL) {
              file result = search_file_in_directory(tok, rest);

              if (result.exists) {
                exists = 1;
                printf("%s is %s\n", rest, result.path);
                break;
              }

              // TODO: (this isn't a task, but rather it's a lesson, I forgot to assign the strtok and so it stayed in an endless loop which I didn't manage to figure out how to solve properly for a long time)
              tok = strtok(NULL, ":");
              
              if (tok == NULL)
                printf("%s: not found\n", rest);
            }
          } else {
            printf("%s: not found\n", rest);
          }
        }
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

file search_file_in_directory(const char *dir_path, const char *target_filename) {
    file result = {{0}, 0};
    
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return result;  // Don't print error, directory might not exist
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Check if this is the file we're looking for
        if (strcmp(entry->d_name, target_filename) == 0) {
            // Build full path
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

            struct stat statbuf;
            if (stat(full_path, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
                // Check if executable
                if (statbuf.st_mode & S_IXUSR) {
                    strncpy(result.path, full_path, sizeof(result.path) - 1);
                    result.path[sizeof(result.path) - 1] = '\0';
                    result.exists = 1;
                    closedir(dir);
                    return result;
                }
            }
        }
    }

    closedir(dir);
    return result;
}