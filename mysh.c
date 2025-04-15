// Fully integrated shell with:
// - Built-in and external command support
// - Piping, redirection (<, >)
// - and/or conditional execution
// - `which` command implementation
// - Wildcard expansion using opendir/readdir

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <limits.h>
#include <fnmatch.h>  
#include <linux/limits.h>



#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define MAX_INPUT_SIZE 1024

const char *search_paths[] = {"/usr/local/bin", "/usr/bin", "/bin"};

typedef struct {
    char **args;
    char *input_file;
    char *output_file;
} ParsedCommand;

char *read_line();
char **split_line(char *line);
char **expand_wildcards(char *token, int *argc);
ParsedCommand *parse_command(char **tokens);
int execute(ParsedCommand *cmd);
int handlecd(char **args);
int handlewhich(char **args);
void handlepwd();
void handleexit();
void handledie();
int handle_external(ParsedCommand *cmd);
int execute_pipe(char **tokens);

void loop(int *interactive)
{
    char *line;
    char **tokens;
    ParsedCommand *cmd;
    int last_success = 1;

    while (1)
    {
        if (*interactive) {
            printf("mysh> ");
            fflush(stdout);
        }

        line = read_line();
        if (line == NULL) break;

        tokens = split_line(line);
        char **original_tokens = tokens;

        if (tokens[0] == NULL) {
            free(tokens); free(line);
            continue;
        }

        bool run = true;
        if (strcmp(tokens[0], "and") == 0) {
            run = last_success;
            tokens++;
        } else if (strcmp(tokens[0], "or") == 0) {
            run = !last_success;
            tokens++;
        }

        bool has_pipe = false;
        for (int i = 0; tokens[i] != NULL; i++) {
            if (strcmp(tokens[i], "|") == 0) {
                has_pipe = true;
                break;
            }
        }

        if (run) {
            if (has_pipe) {
                last_success = execute_pipe(tokens);
            } else {
                cmd = parse_command(tokens);
                if (cmd->args[0] != NULL) {
                    last_success = execute(cmd);
                }
                free(cmd->args); free(cmd);
            }
        }

        free(original_tokens);
        free(line);
    }

    if (*interactive)
        printf("Exiting my shell.\n");
}

char *read_line()
{
    int bufsize = MAX_INPUT_SIZE;
    char *buffer = malloc(sizeof(char) * bufsize);
    int position = 0;
    char c;
    int bytes_read;

    while ((bytes_read = read(0, &c, 1)) > 0)
    {
        if (c == '\n') break;
        buffer[position++] = c;
        if (position >= bufsize)
            buffer = realloc(buffer, bufsize *= 2);
    }

    if (bytes_read == 0 && position == 0) {
        free(buffer);
        return NULL;
    }

    buffer[position] = '\0';
    return buffer;
}

char **split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token = strtok(line, LSH_TOK_DELIM);

    while (token != NULL) {
        if (strchr(token, '*')) {
            int wc_count = 0;
            char **expanded = expand_wildcards(token, &wc_count);
            for (int j = 0; j < wc_count; j++) {
                tokens[position++] = expanded[j];
                if (position >= bufsize) tokens = realloc(tokens, (bufsize += LSH_TOK_BUFSIZE) * sizeof(char *));
            }
            free(expanded);
        } else {
            tokens[position++] = strdup(token);
            if (position >= bufsize) tokens = realloc(tokens, (bufsize += LSH_TOK_BUFSIZE) * sizeof(char *));
        }
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

ParsedCommand *parse_command(char **tokens)
{
    ParsedCommand *cmd = malloc(sizeof(ParsedCommand));
    cmd->args = malloc(LSH_TOK_BUFSIZE * sizeof(char *));
    cmd->input_file = NULL;
    cmd->output_file = NULL;

    int arg_pos = 0;
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "<") == 0 && tokens[i + 1]) {
            cmd->input_file = tokens[++i];
        } else if (strcmp(tokens[i], ">") == 0 && tokens[i + 1]) {
            cmd->output_file = tokens[++i];
        } else {
            cmd->args[arg_pos++] = tokens[i];
        }
    }
    cmd->args[arg_pos] = NULL;
    return cmd;
}

int execute(ParsedCommand *cmd)
{
    int saved_stdout = -1;
    int out_fd = -1;
    if (cmd->output_file) {
        out_fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (out_fd < 0) { perror("output redirection"); return 0; }
        saved_stdout = dup(STDOUT_FILENO);
        dup2(out_fd, STDOUT_FILENO);
    }

    int result = 1;
    if (strcmp(cmd->args[0], "cd") == 0)
        result = handlecd(cmd->args + 1);
    else if (strcmp(cmd->args[0], "pwd") == 0)
        handlepwd();
    else if (strcmp(cmd->args[0], "exit") == 0)
        handleexit();
    else if (strcmp(cmd->args[0], "die") == 0)
        handledie();
    else if (strcmp(cmd->args[0], "which") == 0)
        result = handlewhich(cmd->args + 1);
    else
        result = handle_external(cmd);

    if (cmd->output_file) {
        fflush(stdout);
        dup2(saved_stdout, STDOUT_FILENO);
        close(out_fd);
        close(saved_stdout);
    }
    return result;
}

int handlecd(char **args)
{
    if (args[0] == NULL) { fprintf(stderr, "cd: expected one argument\n"); return 0; }
    if (args[1] != NULL) { fprintf(stderr, "cd: too many arguments\n"); return 0; }
    if (chdir(args[0]) == -1) { perror("cd"); return 0; }
    return 1;
}

int handlewhich(char **args)
{
    if (args[0] == NULL || args[1] != NULL) return 0;
    const char *builtins[] = {"cd", "pwd", "which", "exit", "die"};
    for (int i = 0; i < 5; i++) if (strcmp(args[0], builtins[i]) == 0) return 0;
    for (int i = 0; i < 3; i++) {
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", search_paths[i], args[0]);
        if (access(fullpath, X_OK) == 0) {
            printf("%s\n", fullpath);
            return 1;
        }
    }
    return 0;
}

void handlepwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
    else perror("pwd");
}

void handleexit() { exit(0); }
void handledie() { exit(1); }

int handle_external(ParsedCommand *cmd)
{
    char *path = NULL;
    if (strchr(cmd->args[0], '/')) {
        if (access(cmd->args[0], X_OK) == 0) path = cmd->args[0];
    } else {
        for (int i = 0; i < 3; i++) {
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", search_paths[i], cmd->args[0]);
            if (access(fullpath, X_OK) == 0) {
                path = strdup(fullpath);
                break;
            }
        }
    }

    if (!path) { fprintf(stderr, "%s: command not found\n", cmd->args[0]); return 0; }

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); return 0; }
    if (pid == 0) {
        if (cmd->input_file) {
            int in = open(cmd->input_file, O_RDONLY);
            if (in < 0) { perror("input redirection"); exit(1); }
            dup2(in, STDIN_FILENO); close(in);
        }
        execv(path, cmd->args);
        perror("execv"); exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (path != cmd->args[0]) free(path);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
}

char **expand_wildcards(char *token, int *argc)
{
    DIR *d = opendir(".");
    struct dirent *de;
    char **matches = malloc(sizeof(char *));
    *argc = 0;

    while ((de = readdir(d)) != NULL) {
        if (fnmatch(token, de->d_name, 0) == 0) {
            matches = realloc(matches, (++(*argc)) * sizeof(char *));
            matches[*argc - 1] = strdup(de->d_name);
        }
    }
    closedir(d);

    if (*argc == 0) {
        matches[0] = strdup(token);
        *argc = 1;
    }
    return matches;
}

int execute_pipe(char **tokens)
{
    int i = 0;
    while (tokens[i] && strcmp(tokens[i], "|") != 0) i++;
    if (!tokens[i] || !tokens[i + 1]) { fprintf(stderr, "Syntax error\n"); return 1; }

    tokens[i] = NULL;
    char **left = tokens;
    char **right = &tokens[i + 1];

    ParsedCommand *cmd1 = parse_command(left);
    ParsedCommand *cmd2 = parse_command(right);

    int pipefd[2];
    pipe(pipefd);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        handle_external(cmd1); exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        handle_external(cmd2); exit(1);
    }

    close(pipefd[0]); close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    int status; waitpid(pid2, &status, 0);
    free(cmd1->args); free(cmd1);
    free(cmd2->args); free(cmd2);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char **argv)
{
    int interactive = 1;
    if (argc == 2) {
        int fd = open(argv[1], O_RDONLY);
        if (fd < 0) { perror("open"); exit(EXIT_FAILURE); }
        dup2(fd, STDIN_FILENO); close(fd);
    }
    interactive = isatty(STDIN_FILENO);
    if (interactive) printf("Welcome to my Shell! \n");
    loop(&interactive);
    return EXIT_SUCCESS;
}