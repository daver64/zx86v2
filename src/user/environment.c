#include "common.h"
#include <stdlib.h>

typedef struct environment_t
{
    char *key;
    char *value;
} environment_t;

#define MAX_ENVIRONMENT_VARIABLES 1024

environment_t *envvars[MAX_ENVIRONMENT_VARIABLES];
int envvars_inited = false;

int unsetenv(const char *name)
{
    if (envvars_inited)
    {
        for (int i = 0; i < MAX_ENVIRONMENT_VARIABLES; i++)
        {
            if (envvars[i] != NULL)
            {
                if (!strcmp(envvars[i]->key, name))
                {
                    free(envvars[i]->key);
                    free(envvars[i]->value);
                    free(envvars[i]);
                    envvars[i] = NULL;
                    return 0;
                }
            }
        }
    }
    return 0;
}
void printenvvars()
{
    if(envvars_inited)
    {
         for (int i = 0; i < MAX_ENVIRONMENT_VARIABLES; i++)
        {
            if (envvars[i] != NULL)
            {
                printf("%s=%s\n",envvars[i]->key,envvars[i]->value);
            }
        }       
    }
}
char *getenv(const char *name)
{
    if (envvars_inited)
    {
        for (int i = 0; i < MAX_ENVIRONMENT_VARIABLES; i++)
        {
            if (envvars[i] != NULL)
            {
                if (!strcmp(envvars[i]->key, name))
                {
                    return envvars[i]->value;
                }
            }
        }
    }
    return NULL;
}
int setenv(const char *name, const char *val, int overwrite)
{
    int alreadyexists = false;
    char *e = getenv(name);
    if (e != NULL && overwrite == 0)
    {
        return 0;
    }
    else if (e != NULL && overwrite != 0)
    {
        alreadyexists = true;
    }

    if (!envvars_inited)
    {
        for (int i = 0; i < MAX_ENVIRONMENT_VARIABLES; i++)
        {
            envvars[i] = NULL;
        }
        envvars_inited = true;
    }
    int index = 0;
    environment_t *envvar = envvars[index];
    while (envvar != NULL && index < 128)
    {
        index++;
        envvar = envvars[index];
    }
    if (index < 128)
    {
        // printf("setting enviroment to index %d key=%s, value=%s\n",index,name,val);
        if (alreadyexists)
        {
            free(envvars[index]->key);
            free(envvars[index]->value);
        }
        else
        {
            envvars[index] = malloc(sizeof(environment_t));
        }
        envvars[index]->key = malloc(strlen(name) + 1);
        envvars[index]->value = malloc(strlen(val) + 1);
        envvars[index]->key = strcat(envvars[index]->key, name);
        envvars[index]->value = strcat(envvars[index]->value, val);
    }
    return -1;
}

int putenv(const char *str)
{
    char **args;
    int argc;
    argc = string_split(str, '=', &args);
    if (argc == 2)
    {
        setenv(args[0], args[1], 1);
    }
    for (int i = 0; i < argc; i++)
    {
        free(*(args + i));
    }
    free(args);
}