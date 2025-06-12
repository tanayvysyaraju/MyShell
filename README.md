# mysh: Custom POSIX Shell Implementation

mysh is a C-based custom shell developed using low-level POSIX system calls such as `read()`, `fork()`, `execv()`, `dup2()`, and `pipe()`. It supports both interactive and batch modes and implements a range of features including built-in commands, redirection, pipelines, wildcard expansion, and conditional execution.

---

## Project Motivation

System programming often involves:

* Managing low-level input/output
* Forking and executing processes
* Handling file descriptors and process control

mysh addresses these challenges by providing:

* Unified command parsing and execution pipeline
* Built-in error handling and fallbacks
* Simulated shell environment without external libraries

---

## Features

### Shell Modes

* **Interactive Mode**: Detects terminal input using `isatty()` and provides a prompt (`mysh> `).
* **Batch Mode**: Accepts input from a file or piped stdin and suppresses prompts.

### Input Handling

* Uses `read()` with dynamic memory allocation to support arbitrary-length lines.
* Command execution begins only after the full line is read (up to newline).

### Tokenization

* Uses `strtok()` with standard whitespace delimiters.
* Special symbols (`<`, `>`, `|`) must be whitespace-separated and are parsed distinctly.

### Built-in Commands

* `cd`: Changes directory using `chdir()`.
* `pwd`: Displays the current directory using `getcwd()`.
* `which`: Searches `/usr/local/bin`, `/usr/bin`, and `/bin` using `access()`.
* `exit`: Terminates the shell with exit status `0`.
* `die`: Prints arguments and exits with status `1`.

### Redirection

* `>` and `<` symbols redirect output and input respectively.
* Uses `open()` and `dup2()` to redirect file descriptors in the child process.

### External Commands

* Commands with slashes are treated as absolute/relative paths.
* Otherwise, the shell searches in `/usr/local/bin`, `/usr/bin`, and `/bin`.

### Piping

* Supports **exactly one** pipe `|` between two commands.
* Uses `pipe()` and two `fork()`ed children to connect processes.
* `dup2()` used to redirect `stdout` and `stdin` appropriately.

### Wildcard Expansion

* Expands `*` tokens using `opendir()` and `readdir()`.
* Uses `fnmatch()` to match against file patterns.

### Conditional Execution

* Supports `and` and `or` as the first token of a line.
* Behavior is determined based on the success or failure of the previous command.

---

## Tech Stack

| Component     | Tool/Library                                |
| ------------- | ------------------------------------------- |
| Language      | C (GCC)                                     |
| System Calls  | POSIX (read, fork, execv, dup2, pipe, etc.) |
| Pattern Match | fnmatch                                     |
| I/O Handling  | opendir, readdir                            |
| Build System  | Make                                        |

---

## Setup Instructions

### Prerequisites

* POSIX-compliant system (Linux/macOS)
* C compiler (e.g., gcc)

### Compile

```bash
make
```

### Run

```bash
./mysh
```

To use batch mode:

```bash
./mysh < input.txt
```

---

## Testing Strategy

A suite of shell scripts was used to verify all features. Each script targets a specific feature group:

### 1. Built-ins & Navigation

**Objective**: Validate built-in command functionality.

* Use `pwd` to show the current directory.
* Use `cd` for valid/invalid paths and check error reporting.
* Use `exit` and `die` to verify exit behavior.

**Test Script**: `test_builtins.sh`

### 2. Redirection

**Objective**: Validate `<` and `>` redirection operators.

* Redirect `ls` output to a file, then `cat` the result.
* Use `<` to provide input to `cat`.
* Check behavior on non-existent files.

**Test Script**: `test_redirection.sh`

### 3. Piping

**Objective**: Validate communication between two piped commands.

* Use `ls | grep` to filter directory contents.
* Ensure exit status reflects second command.

**Test Script**: `test_pipe.sh`

### 4. Wildcard Expansion

**Objective**: Validate expansion of `*` tokens.

* Run `ls test_env/*.txt` and subdirectory patterns.
* Ensure unmatched wildcards are preserved.

**Test Script**: `test_wildcards.sh`

### 5. Conditional Execution

**Objective**: Test `and`/`or` logic across commands.

* Run `boguscmd or echo fallback`.
* Use `boguscmd and die ...` to test failure path.
* Reject `and/or` as the first command.

**Test Script**: `test_conditionals.sh`

---

## Authors

* **Tanay Vysyaraju** – `tv193`
* **Saroop Makhija** – `ssm229`
