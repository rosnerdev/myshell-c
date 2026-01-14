#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

FileResult *find_executables_with_prefix(const char *prefix, int *count) {
	*count = 0;
	FileResult *matches;
	int capacity = 10;
	matches = calloc(capacity, sizeof(FileResult));
	if (!matches) return NULL;

	char *path_env = getenv("PATH");
	if (!path_env) {
			free(matches);
			return NULL;
	}

	char *path_copy = strdup(path_env);
	if (!path_copy) {
			free(matches);
			return NULL;
	}

	char *dir = strtok(path_copy, ":");
	size_t prefix_len = strlen(prefix);

	while (dir != NULL) {
			DIR *d = opendir(dir);
			if (d) {
				struct dirent *entry;
				while ((entry = readdir(d)) != NULL) {
					if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
						if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
							/* check if executable */
							char full_path[PATH_MAX];
							snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
							if (access(full_path, X_OK) == 0) {
								int is_duplicate = 0;
								for (int i = 0; i < *count; i++) {
									if (strcmp(matches[i].name, entry->d_name) == 0) {
										is_duplicate = 1;
										break;
									}
								}
								if (is_duplicate) continue;

								/* expand array if needed */
								if (*count >= capacity) {
									capacity *= 2;
									FileResult *new_matches = realloc(matches, capacity * sizeof(FileResult));
									if (!new_matches) {
										/* realloc failed; free the original buffer once and cleanup */
										free(matches);
										closedir(d);
										free(path_copy);
										return NULL;
									}
									matches = new_matches;
								}

								/* TODO: make sure strcpy is good/use strncpy instead. */
								strcpy(matches[*count].name, entry->d_name);
								strcpy(matches[*count].path, full_path);
								++*count;
							}
						}
					}
				}
				closedir(d);
		}
		dir = strtok(NULL, ":");
	}

	free(path_copy);

	/* sort the completions' executables' names alphabetically! */
	for (int i = 0; i < *count - 1; ++i) {
		for (int j = i + 1; j < *count; ++j) {
			if (strcmp(matches[i].name, matches[j].name) > 0) {
				FileResult temp = matches[i];
				matches[i] = matches[j];
				matches[j] = temp;
			}
		}
	}

	return matches;
}
