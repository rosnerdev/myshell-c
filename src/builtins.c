#include "shell.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

static void print_history(char **hist_list, int n)
{
    if (n == -1)
    {
        for (int i = 0; hist_list[i] != NULL; ++i)
        {
            printf("\t%d %s\n", i + 1, hist_list[i]);
        }
        return;
    }

    int count, first_one = 0;
    for (count = 0; hist_list[count] != NULL; ++count)
        ;

    first_one = count - n;
    for (; first_one < count; ++first_one)
    {
        printf("\t%d %s\n", first_one + 1, hist_list[first_one]);
    }
}

int handle_builtin(Command *cmd, char **hist_list)
{
    if (cmd->args == NULL || cmd->args[0] == NULL)
    {
        return 0;
    }

    if (!strcmp(cmd->args[0], "exit"))
    {
        exit(0);
    }

    if (!strcmp(cmd->args[0], "history"))
    {
        int n = -1;
        if (cmd->args[1] != NULL && sscanf(cmd->args[1], "%d", &n) != 1)
        {
            printf("woah there\n");
        }
        else
            print_history(hist_list, n);
        return 1;
    }

    if (!strcmp(cmd->args[0], "cd"))
    {
        if (cmd->arg_count < 2)
        {
            chdir(getenv("HOME"));
        }
        else
        {
            if (chdir(cmd->args[1]) < 0)
            {
                printf("cd: %s: No such file or directory\n", cmd->args[1]);
            }
        }
        return 1;
    }

    if (!strcmp(cmd->args[0], "pwd"))
    {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)))
        {
            printf("%s\n", cwd);
        }
        return 1;
    }

    if (!strcmp(cmd->args[0], "echo"))
    {
        int first = 1;
        for (int i = 1; cmd->args[i] != NULL; ++i)
        {
            if (!first)
                printf(" ");
            printf("%s", cmd->args[i]);
            first = 0;
        }
        puts("");

        return 1;
    }

    if (!strcmp(cmd->args[0], "type"))
    {
        if (cmd->arg_count < 2)
            return 1;

        for (int i = 1; i < cmd->arg_count; ++i)
        {
            if (!strcmp(cmd->args[i], "exit") || !strcmp(cmd->args[i], "echo") ||
                !strcmp(cmd->args[i], "type") || !strcmp(cmd->args[i], "pwd") ||
                !strcmp(cmd->args[i], "cd") || !strcmp(cmd->args[i], "history"))
            {
                printf("%s is a shell builtin\n", cmd->args[i]);
            }
            else
            {
                char *path_env = getenv("PATH");
                char *path = strdup(path_env);
                int found = 0;

                if (path_env != NULL)
                {
                    char *tok = strtok(path, ":");
                    while (tok != NULL)
                    {
                        char fullpath[PATH_MAX];
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", tok, cmd->args[i]);
                        if (access(fullpath, X_OK) == 0)
                        {
                            printf("%s is %s\n", cmd->args[i], fullpath);
                            found = 1;
                            break;
                        }
                        tok = strtok(NULL, ":");
                    }
                    if (!found)
                        printf("%s: not found\n", cmd->args[i]);
                }
                else
                {
                    printf("%s: not found\n", cmd->args[i]);
                }

                free(path);
            }
        }

        return 1;
    }

    return 0;
}