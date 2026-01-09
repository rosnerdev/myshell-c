#ifndef UTILS_H
#define UTILS_H

typedef struct {
  char name[256];
  char path[1024];
  int exists;
} FileResult;

FileResult search_file_in_directory(const char *dir_path, const char *target_filename);
FileResult *find_executables_with_prefix(const char *prefix, int *count);

#endif