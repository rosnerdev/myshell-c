#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

FileResult search_file_in_directory(const char *dir_path, const char *target_filename) {
    FileResult result = {{0}, {0}, 0};
    
    DIR *dir = opendir(dir_path);
    if (!dir) {
			return result;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, target_filename) == 0) {
				char full_path[1024];
				snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

				struct stat statbuf;
				if (stat(full_path, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
					// check if executable
					if (statbuf.st_mode & S_IXUSR) {
						const char *name = strdup(entry->d_name);
						strncpy(result.path, full_path, sizeof(result.path) - 1);
						result.path[sizeof(result.path) - 1] = '\0';
						strncpy(result.name, name, sizeof(result.name) - 1);
						result.name[sizeof(result.name) - 1] = '\0';
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
							// Check if executable
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

								// Expand array if needed
								if (*count >= capacity) {
									capacity *= 2;
									FileResult *new_matches = realloc(matches, capacity * sizeof(FileResult));
									if (!new_matches) {
										for (int i = 0; i < *count; i++) {
											free(matches);
										}
										free(matches);
										closedir(d);
										free(path_copy);
										return NULL;
									}
									matches = new_matches;
								}

								// TODO: make sure strcpy is good/use strncpy instead.
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

	for (int i = 0; i < *count - 1; i++) {
		for (int j = i + 1; j < *count; j++) {
			if (strcmp(matches[i].name, matches[j].name) > 0) {
				FileResult temp = matches[i];
				matches[i] = matches[j];
				matches[j] = temp;
			}
		}
	}

	return matches;
}
