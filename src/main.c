#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

typedef struct {
  char path[1024];
  int exists;
} file;

typedef struct {
	char *stdout_file;
	char *stderr_file;
	int stdout_index;
	int stderr_index;
} redirect_info;

int is_builtin(const char *str);
file search_file_in_directory(const char *dir_path, const char *target_filename);
char **parse_args(char *input);
void free_args(char **args);
redirect_info find_stdout_redirect(char **args);

int get_args_len(char **args) {
  int count = 0;
  char **args_ptr = args;

  while (*args_ptr++ != NULL)
    ++count;

  return count;
}

int main(int argc, char *argv[]) {
  // throw  argc and argv to avoid warnings from the compiler
  (void)argc;
  (void)argv;

  // flush after every printf
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

		redirect_info redir = find_stdout_redirect(args);
		if (redir.stdout_index != -1) {
			free(args[redir.stdout_index]);
			args[redir.stdout_index] = NULL;
		}
		
		if (redir.stderr_index != -1) {
			free(args[redir.stderr_index]);
			args[redir.stderr_index] = NULL;
		}

		int saved_stdout = -1;
		if (redir.stdout_file) {
			// Save original stdout
			saved_stdout = dup(STDOUT_FILENO);
			
			// Open file and redirect stdout to it
			int fd = open(redir.stdout_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd == -1) {
				perror("open");
				exit(1);
			}
			if (dup2(fd, STDOUT_FILENO) == -1) {
				perror("dup2");
				exit(1);
			}
			close(fd);
		}

		int saved_stderr = -1;
		if (redir.stderr_file) {
			// Save original stdout
			saved_stderr = dup(STDERR_FILENO);
			
			// Open file and redirect stdout to it
			int fd = open(redir.stderr_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd == -1) {
				perror("open");
				exit(1);
			}
			if (dup2(fd, STDERR_FILENO) == -1) {
				perror("dup2");
				exit(1);
			}
			close(fd);
		}

    int args_len = get_args_len(args);
      
    if (!strcmp(input, "exit"))
      break;
    else if (!strncmp(input, "echo", 4)) {
      // TODO: make the code better and make echo ignore quotes and other stuff!!!
      for (int i = 1; i < args_len; ++i) {
        if (i > 1) printf(" ");
        printf("%s", args[i]);
      }
      puts("");
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
        /* TODO: fixit so that it will only work outside of quotation marks */
        /* TODO: Consecutive whitespace characters (spaces, tabs) inside single quotes are preserved and are not collapsed or used as delimiters. */
        /* TODO: Quoted strings placed next to each other are concatenated to form a single argument. */
        if (chdir(args[1]) < 0) {
          printf("cd: %s: No such file or directory\n", args[1]);
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
				free_args(args);
				free(line);
				return 1;
      }
      
      if (pid == 0) {
				// Child process
				if (redir.stdout_file) {
					if (redir.stdout_file) {
						int fd = open(redir.stdout_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if (fd == -1) {
							perror("open");
							exit(1);
						}
	
						// redirects stdout to my file
						if (dup2(fd, STDOUT_FILENO) == -1) {
							perror("dup2");
							exit(1);
						}
						
						close(fd);
					}
				} else if (redir.stderr_file) {
					if (redir.stderr_file) {
						int fd = open(redir.stderr_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if (fd == -1) {
							perror("open");
							exit(1);
						}
	
						// redirects stdout to my file
						if (dup2(fd, STDERR_FILENO) == -1) {
							perror("dup2");
							exit(1);
						}
						
						close(fd);
					}
				}
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

		if (saved_stdout != -1) {
			if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
				perror("dup2");
				exit(1);
			}
			close(saved_stdout);
		} else if (saved_stderr != -1) {
			if (dup2(saved_stderr, STDERR_FILENO) == -1) {
				perror("dup2");
				exit(1);
			}
			close(saved_stderr);
		}

    goto cleanup_stuff;
command_not_found:
    fprintf(stderr, "%s: command not found\n", args[0]);
cleanup_stuff:
		if (redir.stdout_file)
			free(redir.stdout_file); // free the orphaned filename
		if (redir.stderr_file)
			free(redir.stderr_file); // free the orphaned filename
    free_args(args);
    free(line);
  }

  return 0;
}

int is_builtin(const char *str) {
	// TODO: add more builtins for my own use cases!
  const char *builtins[] = {"exit", "echo", "type", "pwd", "cd"};
  
  for(int i = 0; i < 5; ++i) {
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

/* TODO: FIXIT: instead of strtok- use a state-machine-based tokenizr that consumes each character separately */
/* TODO: FIXIT: make sure that all these limitations such as 256 characters per string, are unlocked if the user wants to- basically make sure to realloc!!! */
char **parse_args(char *input) {
	// TODO: add support for printing environment variables like "echo $PATH" should totally work!
  char **args = calloc(64, sizeof(char *));  // Max 64 args
  if (!args) return NULL;
  char *current_arg = calloc(256, sizeof(char));
  int arg_pos = 0;
  int arg_count = 0;
  int in_single_quote = 0;
  int in_double_quote = 0;
	int in_escape_mode = 0;
  
  for (int i = 0; input[i] != '\0'; i++) {
    char c = input[i];

    if (in_single_quote) {
			if (c == '\'') {
				in_single_quote = 0;  // end quote, but DON'T end argument
			} else {
				current_arg[arg_pos++] = c;  // copy literally
			}
    } else if (in_double_quote) {
      if (in_escape_mode) {
				// Inside double quotes, only these characters are special after backslash: \ " $ ` newline
				const char *possible_escapes = "\\\"$`";
				int found_escape = 0;
				for (int i = 0; possible_escapes[i] != '\0'; ++i) {
					if (c == possible_escapes[i]) found_escape = 1;
				}
				
				if (found_escape) {
					current_arg[arg_pos++] = c;
				} else if (c == '\n') {
					// Backslash-newline is line continuation (removed)
					// Do nothing - the backslash and newline are both removed
				} else {
					// Not a special escape - keep both the backslash AND the character
					current_arg[arg_pos++] = '\\';
					current_arg[arg_pos++] = c;
				}
				in_escape_mode = 0;
			} else if (c == '"') {
				in_double_quote = 0;
			} else if (c == '\\') {
				in_escape_mode = 1;
			} else {
				current_arg[arg_pos++] = c;
			}
    } else if (in_escape_mode) {
			current_arg[arg_pos++] = c;
			in_escape_mode = 0;
		} else {
			// not in quotes
			if (c == '\'') {
				in_single_quote = 1;  // start quote
			} else if (c == '"') {
				in_double_quote = 1;
			} else if (c == ' ' || c == '\t') {
				if (arg_pos > 0) {
					// finish this argument
					current_arg[arg_pos] = '\0';
					args[arg_count++] = strdup(current_arg);
					arg_pos = 0;
				}
				// skip consecutive spaces (do nothing)
			} else if (c == '\\') {
				in_escape_mode = 1;
			} else if (c == '~') {
				const char *env_home = getenv("HOME");
				/* FIXIT: this is very dangerous! what if there is not enough space of the current_arg? this could potentially create segfaults and/or other bugs!!! URGENTLY REPLACE THIS */
				strcpy(current_arg + arg_pos, env_home);
				arg_pos += (int)strlen(env_home);
			} else {
				current_arg[arg_pos++] = c;
			}
    }
  }
  
  // Don't forget the last argument!
  if (arg_pos > 0) {
      current_arg[arg_pos] = '\0';
      args[arg_count++] = strdup(current_arg);
  }
  
  args[arg_count] = NULL;
  free(current_arg);
  
  return args;
}

void free_args(char **args) {
    for (int i = 0; args[i] != NULL; i++)
        free(args[i]);
    free(args);
}

// FIXIT: doesn't handle edge cases like: missing filename, redirection with no spaces around it, etc.
redirect_info find_stdout_redirect(char **args) {
	redirect_info result = {.stdout_file = NULL, .stderr_file = NULL, .stdout_index = -1, .stderr_index = -1};
	int found_redirect = 0;
	int stdout_val = 0, stderr_val = 0;
	
	int i;
	for (i = 0; args[i] != NULL; ++i)	{
		if (found_redirect) {
			if (stdout_val) {
				result.stdout_file = strdup(args[i]);
				result.stdout_index = i-1;
			} else if (stderr_val) {
				result.stderr_file = strdup(args[i]);
				result.stderr_index = i-1;
			}
			break;
		}
		
		if (!strcmp(">", args[i]) || !strcmp("1>", args[i])) {
			found_redirect = 1;
			stdout_val = 1;
		} else if (!strcmp("2>", args[i])) {
			found_redirect = 1;
			stderr_val = 1;
		}
	}

	return result;
}