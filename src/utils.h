#ifndef UTILS_H
#define UTILS_H

typedef struct
{
  char name[256];
  char path[1024];
  int exists;
} FileResult;

FileResult *find_executables_with_prefix(const char *prefix, int *count);

#endif