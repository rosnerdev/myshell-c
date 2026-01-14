#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include "shell.h"
#include "utils.h"

Command parse_args(char *input);
void free_args(char **args);

static struct termios g_orig_termios;

void disable_raw_mode(struct termios *orig) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}

// FIXIT: for now not working for some reason I'll have to fix.
void handle_sigint(int sig) {
    (void)sig;
    disable_raw_mode(&g_orig_termios);
    printf("\n");
    exit(0);
}

void enable_raw_mode(struct termios *orig) {
    struct termios raw;
    
    // get current terminal attributes
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    
    // disable canonical mode and echo
    raw.c_lflag &= ~(ICANON | ECHO);
    
    // apply the new settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


int main(int argc, char *argv[]) {
  // throw argc and argv to avoid warnings from the compiler
  (void)argc;
  (void)argv;

	struct termios orig_termios;

	enable_raw_mode(&orig_termios);
	signal(SIGINT, handle_sigint);
  
	// flush after every printf
  setbuf(stdout, NULL);

  while (1) {
    printf("$ ");
    fflush(stdout);

    char input[100];
		int pos = 0;
		int tab_count = 0;

		// read chars sequentially
    while (1) {
			char ch;
			if (read(STDIN_FILENO, &ch, 1) != 1)
				goto exit_shell;
		
			if (ch == '\r' || ch == '\n') {
				write(STDOUT_FILENO, "\n", 1); 
				input[pos] = '\0';
				break;
			} else if (ch == 127 || ch == 8) {
				// handle backspace
				if (pos > 0) {
					pos--;
					write(STDOUT_FILENO, "\b \b", 3);
				}
			} else if (ch == '\t') {
				input[pos] = '\0';
				
				char completion[100];
				int has_completion = 0;
				int lcp_written = 0;
				
				if (pos > 0 && strncmp(input, "exit", pos) == 0 && pos < 4) {
					strcpy(completion, "exit ");
					has_completion = 1;
				} else if (pos > 0 && strncmp(input, "echo", pos) == 0 && pos < 4) {
					strcpy(completion, "echo ");
					has_completion = 1;
				} else if (pos > 0 && strncmp(input, "cd", pos) == 0 && pos < 2) {
					strcpy(completion, "cd ");
					has_completion = 1;
				} else if (pos > 0 && strncmp(input, "pwd", pos) == 0 && pos < 3) {
					strcpy(completion, "pwd ");
					has_completion = 1;
				} else if (pos > 0 && strncmp(input, "type", pos) == 0 && pos < 4) {
					strcpy(completion, "type ");
					has_completion = 1;
				} else {
					int completions_count = 0;
					FileResult *completions = find_executables_with_prefix(input, &completions_count);
					
					if (completions_count == 0) {
						write(STDOUT_FILENO, "\x07", 1);
					} else if (completions_count == 1) {
						strcpy(completion, completions[0].name);
						strcat(completion, " ");
						tab_count = 0;
						has_completion = 1;
					} else if (completions_count > 1) {
						/* handles LCP completion
						 * since the completions' names are alphabetically sorted-finding the LCP is simply the LCP of the first and last elements
						 */
						const char *a = completions[0].name;
						const char *b = completions[completions_count - 1].name;
						int len_a = (int)strlen(a);
						int len_b = (int)strlen(b);
						int smallest_len = len_a < len_b ? len_a : len_b;
						int lcp_len = 0;
						for (int i = 0; i < smallest_len; ++i) {
								if (a[i] != b[i]) break;
								lcp_len++;
						}

						if (lcp_len > pos) {
							/* write only the additional characters (from pos..lcp_len-1) */
							int add = lcp_len - pos;
							write(STDOUT_FILENO, a + pos, add);
							memcpy(input + pos, a + pos, add);
							pos += add;
							input[pos] = '\0';
							/* mark that we've already written LCP directly */
							lcp_written = 1;
							has_completion = 0;
							tab_count = 0;
						} else {
							if (tab_count == 0) {
								write(STDOUT_FILENO, "\x07", 1);
								tab_count = 1;
							} else if (tab_count == 1) {
								write(STDOUT_FILENO, "\n", 1);
								for (int i = 0; i < completions_count; ++i) {
									if (i != 0) write(STDOUT_FILENO, "  ", 2);
									write(STDOUT_FILENO, completions[i].name, strlen(completions[i].name));
								}

								write(STDOUT_FILENO, "\n$ ", 3);
								write(STDOUT_FILENO, input, pos);
								tab_count = 0;
							}
						}
					}

					if (completions) free(completions);
				}
				
				if (has_completion && !lcp_written) {
					const char *rest = completion + pos;
					write(STDOUT_FILENO, rest, strlen(rest));

					strcpy(input + pos, rest);
					pos += strlen(rest);
				}
			} else if (ch >= 32 && ch < 127) {
				if (pos < 99) {
					input[pos++] = ch;
					write(STDOUT_FILENO, &ch, 1);
				}
			}
			// ignore other control characters
    }

    char *line = strdup(input);
    Command cmd = parse_args(line);
    if (!cmd.args) {
			perror("parse_args");
			return 1;
    }

		if (cmd.arg_count == 0 || cmd.args[0] == NULL) {
			free_args(cmd.args);
			free(line);
			continue;
		}

		// this entire branch needs some kind of replacement due to it being problematic if there's nothing being redirected nor being piped...!
		if (cmd.output_index != -1) {
			free(cmd.args[cmd.output_index]);
			cmd.args[cmd.output_index] = NULL;
		} else if (cmd.error_index != -1) {
			free(cmd.args[cmd.error_index]);
			cmd.args[cmd.error_index] = NULL;
		} else if (cmd.pipe_index != -1) {
			// so basically when doing a regular command that doesn't pipe nor redirects, it goes here and creates a double free and/or segfault!!!
			free(cmd.args[cmd.pipe_index]); 
			cmd.args[cmd.pipe_index] = NULL;
		}

		int saved_stdout = -1;
		if (cmd.output_file) {
			saved_stdout = dup(STDOUT_FILENO); // save original stdout
			int curr = cmd.append_stdout == 1 ? O_APPEND : O_TRUNC;

			/* open file and redirect stdout to it */
			int fd = open(cmd.output_file, O_WRONLY | O_CREAT | curr, 0644);
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
		if (cmd.error_file) {
			// Save original stderr
			saved_stderr = dup(STDERR_FILENO);
			int curr = cmd.append_stderr == 1 ? O_APPEND : O_TRUNC;

			// Open file and redirect stderr to it
			int fd = open(cmd.error_file, O_WRONLY | O_CREAT | curr, 0644);
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
  
		if (cmd.pipe_cmd) {
			int p[2];
			if (pipe(p) < 0) {
					perror("pipe");
					exit(1);
			}
			
			pid_t pid1 = fork();
			if (pid1 == 0) {
					// CHILD 1: First command (left side of |)
					close(p[0]);
					dup2(p[1], STDOUT_FILENO);
					close(p[1]);
					
					// handle first command: could be builtin or external
					if (!handle_builtin(&cmd)) {
							// external command
							execvp(cmd.args[0], cmd.args);
							perror("left_pipe"); // debug error msg for now
							exit(1);
					}
					exit(0); // if builtin succeeded
			}
			
			pid_t pid2 = fork();
			if (pid2 == 0) {
					// CHILD 2: Second command (right side of |)
					close(p[1]);
					dup2(p[0], STDIN_FILENO);
					close(p[0]);

					Command pipe_command = parse_args(cmd.pipe_cmd);
					if (!handle_builtin(&pipe_command)) {
							execvp(pipe_command.args[0], pipe_command.args);
							perror("right_pipe");
							exit(1);
					}
					exit(0);
			}
			
			/* parent process */
			close(p[0]);
			close(p[1]);
			waitpid(pid1, NULL, 0);
			waitpid(pid2, NULL, 0);	
		} else if (!handle_builtin(&cmd)) {
      pid_t pid = fork();
    
      if (pid < 0) {
				perror("fork");
				free_args(cmd.args);
				free(line);
				return 1;
      }
      
      if (pid == 0) {
				// Child process
				if (cmd.output_file) {
					int curr = cmd.append_stdout == 1 ? O_APPEND : O_TRUNC;
					int fd = open(cmd.output_file, O_WRONLY | O_CREAT | curr, 0644);
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
				if (cmd.error_file) {
					int curr = cmd.append_stderr == 1 ? O_APPEND : O_TRUNC;
					int fd = open(cmd.error_file, O_WRONLY | O_CREAT | curr, 0644);
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
				execvp(cmd.args[0], cmd.args);
				// if execvp returns, it failed
				goto command_not_found;

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
		}
		if (saved_stderr != -1) {
			if (dup2(saved_stderr, STDERR_FILENO) == -1) {
				perror("dup2");
				exit(1);
			}
			close(saved_stderr);
		}

    goto cleanup_stuff;
command_not_found:
    fprintf(stderr, "%s: command not found\n", cmd.args[0]);
cleanup_stuff:
		if (cmd.output_file)
			free(cmd.output_file); // free the orphaned filename
		if (cmd.error_file)
			free(cmd.error_file); // free the orphaned filename
		if (cmd.pipe_cmd)
			free(cmd.pipe_cmd);
    free_args(cmd.args);
    free(line);
  }
exit_shell:
	disable_raw_mode(&orig_termios);
  return 0;
}

Command parse_args(char *input) {
	// TODO: add support for printing environment variables like "echo $PATH" should totally work!
  Command result = {.args = NULL, .arg_count = 0, .output_file = NULL, .error_file = NULL, .output_index = -1, .error_index = -1, .pipe_index = -1, .append_stdout = 0, .append_stderr = 0 };

	char **args = calloc(64, sizeof(char *));  // Max 64 args
  if (!args) return result;
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
				} else {
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

	result.args = args;
	result.arg_count = arg_count;
	
	int found_redirect = 0, found_append = 0, found_pipe = 0;
	int output_val = 0, error_val = 0;
	
	int i;
	for (i = 0; args[i] != NULL; ++i)	{
		if (found_redirect) {
			if (output_val) {
				result.output_file = strdup(args[i]);
				result.output_index = i-1;
			} else if (error_val) {
				result.error_file = strdup(args[i]);
				result.error_index = i-1;
			}
			break;
		}
		
		if (found_append) {
			if (output_val) {
				result.output_file = strdup(args[i]);
				result.output_index = i-1;
				result.append_stdout = 1;
			} else if (error_val) {
				result.error_file = strdup(args[i]);
				result.error_index = i-1;
				result.append_stderr = 1;
			}
			break;
		}
		
		if (found_pipe) {
			/* weird patched-together solution btw - comment for ai chatbot */
			/* WARNING: if the result is larger than 512 then it will not work probably... */
			int k = i;
			result.pipe_cmd = calloc(513, sizeof(char));
			while (args[k] != NULL) {
				strcat(result.pipe_cmd, args[k]);
				strcat(result.pipe_cmd, " ");
				++k;
			}
			result.pipe_index = i-1;
			break;
		}
		
		 if (!strcmp("1>>", args[i]) || !strcmp(">>", args[i])) {
			found_append = 1;
			output_val = 1;
		} else if (!strcmp("2>>", args[i])) {
			found_append = 1;
			error_val = 1;
		} else if (!strcmp(">", args[i]) || !strcmp("1>", args[i])) {
			found_redirect = 1;
			output_val = 1;
		} else if (!strcmp("2>", args[i])) {
			found_redirect = 1;
			error_val = 1;
		} else if (!strcmp("|", args[i])) {
			found_pipe = 1;
		}
	}


  return result;
}

void free_args(char **args) {
    for (int i = 0; args[i] != NULL; i++)
        free(args[i]);
    free(args);
}