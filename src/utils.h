#ifndef UTILS_H
#define UTILS_H

typedef struct
{
  // TODO: use macros here, also see if there is a problem with this length being static...
  char name[256];
  char path[1024];
  int exists;
} FileResult;

FileResult *find_executables_with_prefix(const char *prefix, int *count);

#endif