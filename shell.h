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

// parse
int parseCommand(char** result, char* input);
void findVar(char *str);

// execute
int executeCommands(int argc, char** command);

int launchNewProcess(char* path, char *argv[]);
void addHistory(char *command);
int redirect();

void freeStructMem();

// built-in commands, for comparing with input
// exit does not ha
char *builtInCommands[] = {
    "cd",
    "export",
    "local",
    "vars",
    "history",
    "ls",
    "exit"
};
// array size of built-in commands
int builtInCount = 7;

// function pointer for built-in commands
int (*builtInCommandFunc[])(int argc, char *argv[]) = {
    [0] = cd,
    [1] = export,
    [2] = local,
    [3] = vars,
    [4] = history,
    [5] = ls
};
