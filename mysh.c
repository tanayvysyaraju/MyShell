#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

char *read_line();
char **split_line();
int execute(char **args);
int handlecd(char **args);
void handlels();
void handlepwd();
void handleexit();
void handledie();

void loop(int *interactive)
{
    char *line;
    char **m;
    int status = 1;

    while (status)
    {
        if (*interactive)
        {
            printf("mysh> ");
            fflush(stdout);
        }
        else
        {
            printf(" ");
        }

        line = read_line();
        m = split_line(line);
        status = execute(m);

        free(line);
        free(m);
    }
}

char *read_line()
{
    int bufsize = 1024;
    char *buffer = malloc(sizeof(char) * bufsize);
    int position = 0;
    char c;
    int bytes_read;

    while ((bytes_read = read(0, &c, 1)) > 0)
    {
        if (c == '\n')
            break;

        buffer[position] = c;
        position++;

        if (position >= bufsize)
        {
            bufsize *= 2;
            buffer = realloc(buffer, bufsize);
        }
    }

    buffer[position] = '\0';
    return buffer;
}

char **split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL)
    {
        tokens[position++] = token;

        if (position >= bufsize)
        {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int execute(char **args)
{
    // printf("%s", args[0]);
    if (args[0] == NULL)
        return EXIT_FAILURE;

    if (strcmp(args[0], "cd") == 0)
        return handlecd(args + 1);
    else if (strcmp(args[0], "ls") == 0)
        handlels();
    else if (strcmp(args[0], "pwd") == 0)
        handlepwd();
    else if (strcmp(args[0], "exit") == 0)
        handleexit();
    else if (strcmp(args[0], "die") == 0)
        handledie();

    return 0;
}

int handlecd(char **args)
{   
    if(args[0] == NULL) {fprintf(stderr, "cd: Too many arguments\n");return 0;}
    else if(args[1]!=NULL) {fprintf(stderr, "cd: Too many arguments\n");return 0;}
    else{
    int ret = chdir(args[0]);
    if (ret == -1){perror("cd"); return -1;}
    else{ return 1;}
    }
}

// Dummy stubs for other built-ins
void handlels() { 
    printf("ls command not yet implemented\n"); 
}
void handlepwd() { printf("pwd command not yet implemented\n"); }
void handleexit() { printf("exit command not yet implemented\n"); exit(0); }
void handledie() { printf("die command not yet implemented\n"); exit(1); }



int main(int argc, char **argv)
{
    int interactive;
    if (isatty(0))
    {
        printf("Welcome to my Shell! \n");
        interactive = 1;
    }
    else
    {
        interactive = 0;
    }

    loop(&interactive);
    return EXIT_SUCCESS;

}
