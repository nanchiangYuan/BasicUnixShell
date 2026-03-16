# Basic Unix Shell

A lightweight Unix shell implementation written in C, designed to demonstrate core concepts of process management, file descriptor manipulation, and system architecture.

## Features
 + **Command Execution & Process Control**:
    + Dual Execution Logic: Capable of executing both internal built-ins commands and external system binaries  
    + Process Lifecycle Management: Uses `fork()`, `execv()`, and `wait()` to handle child process creation and synchronization
    + Command Resolution Priority: 
        1. Shell built-ins: internal functions executed within the parent process
        2. Explicit paths: execution via full or relative paths
        3. Path search: searches through directories defined in `$PATH` (initialized to `/bin` at startup)  

 + **I/O Redirection**:
    + Implements stream redirection by manipulating file descriptors using `dup2()`:

      - Input Redirection (<): Reads input from a file instead of the keyboard
      - Output Redirection (>): Overwrites a file with the command's output
      - Appending Output (>>): Appends the command's output to the end of a file
      - Redirect Output & Error (&>): Captures both stdout and stderr into a single file
      - Append Output & Error (&>>): Appends both stdout and stderr to a file

 + **Environment & Shell Variables**:
    + Scoped Variable Management: supports the assignment of local shell variables and global environment variables
    + Variable substitution: expands references (e.g., `$VAR`) to their stored values before command execution

 + **Batch Processing**:
    + Non-Interactive Execution: supports batch processing by reading commands from external scripts
    + Comment Support: recognizes and ignores lines starting with `#`, allowing for documented scripts

 + **Command History**:
    + Features a persistent history system to track and recall previously executed commands  

 + **Error Handling**: 
    + Resistant to any possible error conditions including failed syscall, failed commands, bad parameters, etc.

## Project Structure
 + `main.c`: where the command line loop is run
 + `parse.c`: parses the commands and does variable substitution
 + `execute.c`: executes the commands and deals with redirection and forking
 + `commands.c`: build-in commands and helper functions
 + `bsh.h`: contains constants, data structures, and function signatures

## Usage
### Compile
1. On Linux, use `make all` to compile the files. It will create 2 executables, one of which is for debugging.
2. If you just want one executable, you can do `make bsh`, or `make bsh-dbg` for debugging.
3. Use `make clean` to remove the executables.  

### Interactive Mode
1. Run the shell with `./bfs`. 
2. If you see `bsh> ` then you are in!  

### Batch Mode
 + Direct Execution:
    1. Create a `script.bsh` file (or wahtever name you want) that includes the commands you want to run.  
       Example:  
       ```  
       echo hello
       ls
       ps
       ```  
    2. Run `./wsh script.bsh`
 + SheBang:
    1. Create a `script.bsh` file (or wahtever name you want) that includes the commands you want to run with a shebang on top.  
       Example:  
       ```  
       #!./bsh

       echo hello
       ```  
    2. Run `script.bsh`
       (Remember to change permissions of the script)

### Demo
    ```
    bsh> qwe
    bsh: command not found: qwe
    bsh> ls
    Makefile
    README.md
    bsh
    bsh-dbg
    bsh.h
    bsh.h
    commands.c
    execute.c
    main.c
    p3.png
    parse.c
    script1.bsh
    bsh> export AAA=5
    bsh> echo $AAA
    5
    bsh> local b=123
    bsh> local c=987
    bsh> local f=$b
    bsh> vars
    b=123
    c=987
    f=123
    bsh> local b=
    bsh> vars
    b=
    c=987
    f=123
    bsh> local q=ps
    bsh> $q
        PID TTY          TIME CMD
        441 pts/2    00:00:00 bash
    1776 pts/2    00:00:00 bsh
    1802 pts/2    00:00:00 ps
    bsh> mkdir dir1
    bsh> cd dir1
    bsh> pwd
    /home/ashley_yuan/Projects/BasicUnixShell/dir1
    bsh> cd ..
    bsh> history
    1) pwd
    2) mkdir dir1
    3) $q
    4) echo $AAA
    5) qwe
    bsh> history 3
        PID TTY          TIME CMD
        441 pts/2    00:00:00 bash
    1776 pts/2    00:00:00 bsh
    1817 pts/2    00:00:00 ps
    bsh> history set 8
    bsh> sleep 1
    bsh> echo hello &>>text.txt
    bsh> cat <text.txt
    hello
    bsh> history
    1) cat <text.txt
    2) echo hello &>>text.txt
    3) sleep 1
    4) pwd
    5) mkdir dir1
    6) $q
    7) echo $AAA
    8) qwe
    bsh> rm text.txt
    bsh> sleep 2
    bsh> history
    1) sleep 2
    2) rm text.txt
    3) cat <text.txt
    4) echo hello &>>text.txt
    5) sleep 1
    6) pwd
    7) mkdir dir1
    8) $q
    bsh> history set 3
    bsh> history
    1) sleep 2
    2) rm text.txt
    3) cat <text.txt
    bsh> ps >out.txt
    bsh> cat out.txt
        PID TTY          TIME CMD
        441 pts/2    00:00:00 bash
    1776 pts/2    00:00:00 bsh
    1846 pts/2    00:00:00 ps
    bsh> pwd >>out.txt
    bsh> history
    1) pwd >>out.txt
    2) cat out.txt
    3) ps >out.txt
    bsh> history 2
        PID TTY          TIME CMD
        441 pts/2    00:00:00 bash
    1776 pts/2    00:00:00 bsh
    1846 pts/2    00:00:00 ps
    /home/ashley_yuan/Projects/BasicUnixShell
    bsh> exit
    ```  

### Built-in Commands
 + `exit`: calls `exit()` to exit the shell
 + `cd`: changes directories using `chdir()` system call
 + `export`: uses `setenv()` to create or assign environment variables
             usage: `export VAR=<value>` with `VAR` being the variable name
 + `local`: creates or assigns shell variables and stores them in an internal linked list
            usage: `local VAR=<value>` with `VAR` being the variable name
 + `vars`: prints all shell variables and their values in the format `<var>=<value>`
 + `history`: prints the commands used, by default it can store 5 commands
              - `history set <n>`: configures the length of the history, n is an integer
              - `history <n>`: executes the nth command in history. if a command is executed this way, it is not added to the history
 + `ls`: prints the same output as `LANG=C ls -1 --color=never`, uses `opendir()` and `readdir()`  

## References
I looked at the code and how things are done in these websites:
 + https://brennan.io/2015/01/16/write-a-shell-in-c/
 + https://stackoverflow.com/questions/11042218/c-restore-stdout-to-terminal

I also looked at the man pages of various functions, and the xv6 files



