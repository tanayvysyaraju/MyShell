#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h> 

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
int handle_external(char **args);

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
        

        line = read_line();
        if(line== NULL) break;
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
     if (bytes_read == 0 && position == 0) {
            free(buffer);
            return NULL; // signal EOF
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
    else if (strcmp(args[0], "pwd") == 0)
        handlepwd();
    else if (strcmp(args[0], "exit") == 0)
        handleexit();
    else if (strcmp(args[0], "die") == 0)
        handledie();
    else{
        return handle_external(args);
    }
    return 0;
}

int handlecd(char **args)
{   
    if(args[0] == NULL) {fprintf(stderr, "cd: expected one argument\n");return 0;}
    else if(args[1]!=NULL) {fprintf(stderr, "cd: Too many arguments\n");return 0;}
    else{
    int ret = chdir(args[0]);
    if (ret == -1){perror("cd"); return -1;}
    else{ return 1;}
    }
}

int handle_external(char **args)
{
    if (args[0] == NULL) return 0;

    char *path = NULL;

    // If args[0] contains a '/', use it directly
    if (strchr(args[0], '/') != NULL) {
        if (access(args[0], X_OK) == 0) {
            path = args[0];
        } else {
            fprintf(stderr, "%s: command not found or not executable\n", args[0]);
            return 0;
        }
    } else {
       
        const char *dirs[] = {"/usr/local/bin", "/usr/bin", "/bin"};
        char fullpath[1024];
        for (int i = 0; i < 3; i++) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dirs[i], args[0]);
            if (access(fullpath, X_OK) == 0) {
                path = strdup(fullpath);  // dynamically allocated
                break;
            }
        }

        if (path == NULL) {
            fprintf(stderr, "%s: command not found\n", args[0]);
            return 0;
        }
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        handledie();
    } else if (pid == 0) {
        // Child process
        execv(path, args);
        perror("execv");
        handledie();
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (path != args[0]) free(path); 

       
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return 1;
        else
            return 0;
    }
}


void handlepwd() { printf("pwd command not yet implemented\n"); }
void handleexit() {exit(0);}
void handledie() {exit(1);}



int main(int argc, char **argv)
{
    int interactive;

    // If a file is provided, open and redirect stdin
    if (argc == 2) {
        int fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, 0); // redirect stdin to the file
        close(fd);
    }

    if (isatty(0)) {
        printf("Welcome to my Shell! \n");
        interactive = 1;
    } else {
        interactive = 0;
    }

    loop(&interactive);
    return EXIT_SUCCESS;
}
