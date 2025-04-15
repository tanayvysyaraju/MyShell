#define _POSIX_C_SOURCE 200809L
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
#include <sys/stat.h>

#define MAX_INPUT_SIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

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
void handledie(char **args);
int handle_external(ParsedCommand *cmd);
int execute_pipe(char **tokens);

void loop(int *interactive) {
    char *line;
    char **tokens;         // pointer returned by split_line()
    ParsedCommand *cmd;
    int last_success = 1;
    bool is_first_command = true; // used to prevent conditionals as the first token

    while (1) {
        if (*interactive) {
            printf("mysh> ");
            fflush(stdout);
        }

        line = read_line();
        if (line == NULL)
            break;

        // Remove comments: ignore everything after '#' if not inside quotes.
        bool in_single_quote = false;
        bool in_double_quote = false;
        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == '\"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
            } else if (line[i] == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
            } else if (line[i] == '#' && !in_single_quote && !in_double_quote) {
                line[i] = '\0';
                break;
            }
        }

        tokens = split_line(line);
        if (tokens[0] == NULL) {
            free(tokens);
            free(line);
            continue;
        }

        // Prevent conditionals as the very first command.
        if (is_first_command &&
           (strcmp(tokens[0], "and") == 0 || strcmp(tokens[0], "or") == 0)) {
            fprintf(stderr, "Syntax error: conditional command cannot be first\n");
            free(tokens);
            free(line);
            continue;
        }

        // Use an offset rather than incrementing the tokens pointer.
        int offset = 0;
        bool run = true;
        if (strcmp(tokens[0], "and") == 0) {
            run = last_success;
            offset = 1;
        } else if (strcmp(tokens[0], "or") == 0) {
            run = !last_success;
            offset = 1;
        }

        // Check for presence of pipe in the command tokens (after offset).
        bool has_pipe = false;
        for (int i = offset; tokens[i] != NULL; i++) {
            if (strcmp(tokens[i], "|") == 0) {
                has_pipe = true;
                break;
            }
        }

        if (run) {
            if (has_pipe) {
                last_success = execute_pipe(tokens + offset);
            } else {
                cmd = parse_command(tokens + offset);
                if (cmd->args[0] != NULL) {
                    last_success = execute(cmd);
                }
                free(cmd->args);
                free(cmd);
            }
        }
        free(tokens); // free the original pointer returned by split_line()
        free(line);
        is_first_command = false;
    }

    if (*interactive)
        printf("Exiting my shell.\n");
}

char *read_line() {
    int bufsize = MAX_INPUT_SIZE;
    char *buffer = malloc(bufsize);
    int position = 0;
    char c;
    int bytes_read;

    while ((bytes_read = read(0, &c, 1)) > 0) {
        if (c == '\n')
            break;
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

char **split_line(char *line) {
    int bufsize = LSH_TOK_BUFSIZE;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    int i = 0;

    while (line[i] != '\0') {
        while (isspace(line[i]))
            i++; // skip whitespace
        if (line[i] == '\0')
            break;

        char *start;
        char *token;
        int len = 0;

        // Quoted token
        if (line[i] == '\'' || line[i] == '\"') {
            char quote = line[i++];
            start = &line[i];
            while (line[i] != '\0' && line[i] != quote) {
                i++;
                len++;
            }
            token = malloc(len + 1);
            strncpy(token, start, len);
            token[len] = '\0';
            if (line[i] == quote)
                i++; // skip closing quote
        }
        // Unquoted token
        else {
            start = &line[i];
            while (line[i] != '\0' && !isspace(line[i])) {
                i++;
                len++;
            }
            token = malloc(len + 1);
            strncpy(token, start, len);
            token[len] = '\0';
        }

        tokens[position++] = token;
        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
        }
    }

    tokens[position] = NULL;
    return tokens;
}

char **expand_wildcards(char *pattern, int *argc) {
    char *slash = strrchr(pattern, '/');
    char dirpath[PATH_MAX];
    const char *filename_pattern;

    if (slash) {
        size_t len = slash - pattern;
        strncpy(dirpath, pattern, len);
        dirpath[len] = '\0';
        filename_pattern = slash + 1;
    } else {
        strcpy(dirpath, ".");
        filename_pattern = pattern;
    }

    DIR *d = opendir(dirpath);
    if (!d)
        return NULL;

    struct dirent *de;
    char **matches = malloc(sizeof(char *));
    *argc = 0;

    while ((de = readdir(d)) != NULL) {
        if (fnmatch(filename_pattern, de->d_name, 0) == 0) {
            char fullpath[PATH_MAX * 2];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, de->d_name);
            matches = realloc(matches, (++(*argc)) * sizeof(char *));
            matches[*argc - 1] = strdup(fullpath);
        }
    }

    closedir(d);

    if (*argc == 0) {
        matches[0] = strdup(pattern);
        *argc = 1;
    }

    return matches;
}

ParsedCommand *parse_command(char **tokens) {
    int bufsize = LSH_TOK_BUFSIZE;
    ParsedCommand *cmd = malloc(sizeof(ParsedCommand));
    cmd->args = malloc(bufsize * sizeof(char *));
    cmd->input_file = NULL;
    cmd->output_file = NULL;

    int arg_pos = 0;
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "<") == 0 && tokens[i + 1]) {
            cmd->input_file = tokens[++i];
        } else if (strcmp(tokens[i], ">") == 0 && tokens[i + 1]) {
            cmd->output_file = tokens[++i];
        } else {
            // Check for wildcard character.
            if (strchr(tokens[i], '*') != NULL) {
                int matches_count = 0;
                char **matches = expand_wildcards(tokens[i], &matches_count);
                for (int j = 0; j < matches_count; j++) {
                    if (arg_pos >= bufsize) {
                        bufsize *= 2;
                        cmd->args = realloc(cmd->args, bufsize * sizeof(char *));
                    }
                    cmd->args[arg_pos++] = matches[j];
                }
                free(matches);
            } else {
                if (arg_pos >= bufsize) {
                    bufsize *= 2;
                    cmd->args = realloc(cmd->args, bufsize * sizeof(char *));
                }
                cmd->args[arg_pos++] = tokens[i];
            }
        }
    }
    cmd->args[arg_pos] = NULL;
    return cmd;
}

int execute(ParsedCommand *cmd) {
    int saved_stdout = -1;
    int out_fd = -1;

    if (cmd->output_file) {
        out_fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (out_fd < 0) {
            perror("output redirection");
            return 0;
        }
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
        handledie(cmd->args + 1);
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

int handlecd(char **args) {
    if (args[0] == NULL) {
        fprintf(stderr, "cd: expected one argument\n");
        return 0;
    }
    if (args[1] != NULL) {
        fprintf(stderr, "cd: too many arguments\n");
        return 0;
    }
    if (chdir(args[0]) == -1) {
        perror("cd");
        return 0;
    }
    return 1;
}

void handlepwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)))
        printf("%s\n", cwd);
    else
        perror("pwd");
}

void handleexit() {
    exit(0);
}

void handledie(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        printf("%s ", args[i]);
    }
    if (args[0])
        printf("\n");
    exit(1);
}

int handlewhich(char **args) {
    if (args[0] == NULL || args[1] != NULL)
        return 0;

    const char *builtins[] = {"cd", "pwd", "which", "exit", "die"};
    for (int i = 0; i < 5; i++) {
        if (strcmp(args[0], builtins[i]) == 0)
            return 0;
    }

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

int handle_external(ParsedCommand *cmd) {
    char *path = NULL;

    if (strchr(cmd->args[0], '/')) {
        if (access(cmd->args[0], X_OK) == 0)
            path = cmd->args[0];
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

    if (!path) {
        fprintf(stderr, "%s: command not found\n", cmd->args[0]);
        return 0;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 0;
    } else if (pid == 0) {
        if (cmd->input_file) {
            int in = open(cmd->input_file, O_RDONLY);
            if (in < 0) {
                perror("input redirection");
                exit(1);
            }
            dup2(in, STDIN_FILENO);
            close(in);
        }
        /* 
           Only close STDIN when no input redirection is provided and 
           if STDIN appears to be a regular file (i.e. the command was launched in batch mode).
           In a pipeline, STDIN will be a FIFO and should remain open.
        */
        if (cmd->input_file == NULL && !isatty(STDIN_FILENO)) {
            struct stat st;
            if (fstat(STDIN_FILENO, &st) == 0 && S_ISREG(st.st_mode))
                close(STDIN_FILENO);
        }
        execv(path, cmd->args);
        perror("execv");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (path != cmd->args[0])
            free(path);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
}

int execute_pipe(char **tokens) {
    int i = 0;
    while (tokens[i] && strcmp(tokens[i], "|") != 0)
        i++;
    if (!tokens[i] || !tokens[i + 1]) {
        fprintf(stderr, "Syntax error\n");
        return 1;
    }

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
        close(pipefd[0]);
        close(pipefd[1]);
        handle_external(cmd1);
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        handle_external(cmd2);
        exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    int status;
    waitpid(pid2, &status, 0);

    free(cmd1->args);
    free(cmd1);
    free(cmd2->args);
    free(cmd2);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char **argv) {
    int interactive = 1;

    if (argc == 2) {
        int fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    interactive = isatty(STDIN_FILENO);
    if (interactive)
        printf("Welcome to my Shell! \n");

    loop(&interactive);
    return EXIT_SUCCESS;
}
