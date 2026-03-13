#ifndef BSH_H
#define BSH_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

// some max values
#define MAXLENGTH 256
#define MAXARGS 128
#define MAXBUFFER 1024
#define HISTORYCAPACITY 5

// a struct to store history
struct commandHistory{
    char **commands;        // all the commands in history (not broken down to tokens)
    int size;               // the number of commands currently in history
    int capacity;           // the amount of commands the current history can hold
    int mostRecentIndex;    // the index of the most recent command
};

// a struct to store shell variables (linked list)
struct shellVariable{
    char *name;                     // name of the variable
    char *value;                    // value of the variable
    struct shellVariable *nextVar;  // pointer to the next variable
};

// store the file names and file descriptors for redirection
struct redirInfo{
    int fileDes;        // from input
    int fdRestore;      // a copy of fd from input, for restoring
    int mode;           // 0: "<"   
                        // 1: ">"
                        // 2: ">>"
                        // 3: "&>"
                        // 4: "&>>"
    char *fname;        // file name from input
};

// built-in command functions
// return 1: success
//       -1: fail
int cd(int argc, char *argv[]);
int export(int argc, char *argv[]);
int local(int argc, char *argv[]);
int vars(int argc, char *argv[]);
int history(int argc, char *argv[]);
int ls(int argc, char *argv[]);

// command helper
void addHistory(char *command);
void freeStructMem();

// parse
int parseCommand(char** result, char* input);
void findVar(char *str);

// execute
int executeCommands(int argc, char** command);
int launchNewProcess(char* path, char *argv[]);
int redirect();

// built-in commands, for comparing with input
// defined in execute.c
extern char *builtInCommands[];

// array size of built-in commands
// defined in execute.c
extern int builtInCount;

// function pointer for built-in commands
// defined in execute.c
extern int (*builtInCommandFunc[])(int argc, char *argv[]);

// history of commands
extern struct commandHistory* his;

// stores shell variables as a linked list
extern struct shellVariable* shellVarRoot;

// redirection info for the current command
// if mode = -1, no redirection on current command
extern struct redirInfo *redirInfoPtr;

#endif