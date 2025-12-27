#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

typedef struct {
  char path[1024];
  int exists;
} file;

int is_builtin(const char *str);
file search_file_in_directory(const char *dir_path, const char *target_filename);
char **parse_args(char *input);

int get_args_len(char **args) {
  int count = 0;
  char **args_ptr = args;

  while (*args_ptr++ != NULL)
    ++count;

  return count;
}

int main(int argc, char *argv[]) {
  // Throw argc and argv to avoid warnings from the compiler
  (void)argc;
  (void)argv;

  // Flush after every printf
  setbuf(stdout, NULL);

  while (1) {
    printf("$ ");
    
    char input[100];
    if (!fgets(input, 100, stdin))
      break;

    input[strcspn(input, "\n")] = '\0';

    char *line = strdup(input);
    // split the arguments
    char **args = parse_args(line);
    if (!args) {
        perror("parse_args");
        return 1;
    }

    int args_len = get_args_len(args);
      
    if (!strcmp(input, "exit"))
      break;
    else if (!strncmp(input, "echo", 4)) {
      // TODO: make the code better and make echo ignore quotes and other stuff!!!
      // FORMER CODE:
      // if (args_len > 1)
      //   puts(args[1]);
      const char *rest = strchr(input, ' ') + 1;
      if (rest != NULL)
        puts(rest);
    } else if (!strncmp(input, "type", 4) && strlen(input) > 5) {
      if (args_len > 1) {
        if (is_builtin(args[1])) {
          printf("%s is a shell builtin\n", args[1]);
        } else {
          char *path_env = getenv("PATH");
          char *path = strdup(path_env);

          if (path_env != NULL) {
            char *tok = strtok(path, ":");
            while (tok != NULL) {
              file result = search_file_in_directory(tok, args[1]);

              if (result.exists) {
                printf("%s is %s\n", args[1], result.path);
                break;
              }

              // TODO LESSON: (this isn't a task, but rather it's a lesson, I forgot to assign the strtok and so it stayed in an endless loop which I didn't manage to figure out how to solve properly for a long time)
              tok = strtok(NULL, ":");
              
              if (tok == NULL)
                printf("%s: not found\n", args[1]);
            }
          } else {
            printf("%s: not found\n", args[1]);
          }

          free(path); // make sure to free the strdup'd string!
        }
      }
      /* TODO: add an else that makes it print "too little arguments" passed to 'type' */

    /* FIXIT: this whole check is wrong since it still works even if there are more arguments afterwards, so we'll need to refactor this bit. */
    /* TODO: make it so that the args parsing is done before the if-else branch in the top-level of main() */
    } else if (!strncmp(input, "pwd", 3)) {
      /* TODO: if there is more than one argument, print "too many arguments" */
      char cwd[1024];

      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
      } else {
        perror("getcwd");
        return 1;
      }
    } else if (!strncmp(input, "cd", 2)) {
      if (args_len > 1) {
        const char *path_to_use;
        char *dir_to_change = NULL;
        
        // Check if there's a tilde to expand
        if (strchr(args[1], '~') != NULL) {
          dir_to_change = calloc(1024, sizeof(char));
          char *dir_ptr = args[1];
          while (*dir_ptr) {
            size_t len = strcspn(dir_ptr, "~");
            strncpy(dir_to_change, dir_ptr, len);
            dir_to_change[len] = '\0';

            if (*dir_ptr == '~') {
              const char *home = getenv("HOME");
              if (home) {
                strcat(dir_to_change, home);
                dir_ptr++;
              }
            }
            strcat(dir_to_change, dir_ptr);
            dir_ptr += strspn(dir_ptr, "~");
          }
          path_to_use = dir_to_change;
        } else {
          path_to_use = args[1];
        }

        int chdir_result = chdir(path_to_use);
        
        if (chdir_result < 0) {
          printf("cd: %s: No such file or directory\n", path_to_use);
        }
        
        if (dir_to_change) {
          free(dir_to_change);
        }
      }
    } else {
      char *path_env = getenv("PATH");
      char *path = strdup(path_env);

      int not_found_path = 0;

      if (path_env != NULL) {
        char *path_tok_saveptr;
        char *tok = strtok_r(path, ":", &path_tok_saveptr);
        while (tok != NULL) {
          file result = search_file_in_directory(tok, args[0]);

          if (result.exists) {
            break;
          }
          
          tok = strtok_r(NULL, ":", &path_tok_saveptr);
          
          if (tok == NULL) {
            not_found_path = 1;
            break;
          }
        }
      } else {
        not_found_path = 1;
      }
      free(path);

      if (not_found_path)
        goto command_not_found;
        
      pid_t pid = fork();
    
      if (pid < 0) {
          perror("fork");
          free(args);
          free(line);
          return 1;
      }
      
      if (pid == 0) {
          // Child process
          execvp(args[0], args);
          // If execvp returns, it failed
          perror("execvp");
          exit(1);
      } else {
          // Parent process
          int status;
          waitpid(pid, &status, 0);
      }
    }

    goto cleanup_stuff;
command_not_found:
    printf("%s: command not found\n", args[0]);
cleanup_stuff:
    free(args);
    free(line);
  }

  return 0;
}

int is_builtin(const char *str) {
  const char *builtins[] = {"exit", "echo", "type", "pwd", "cd"};
  
  for (int i = 0; i < 5; ++i) {
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

char **parse_args(char *input) {
    char **args = malloc(64 * sizeof(char *));  // Max 64 args
    if (!args) return NULL;
    
    int i = 0;
    char *token = strtok(input, " \t\n");
    while (token != NULL && i < 63) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;  // execvp requires NULL-terminated array
    
    return args;
}
