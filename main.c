#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

// history of commands
struct commandHistory* his;

// stores shell variables as a linked list
struct shellVariable* shellVarRoot;

// redirection info for the current command
// if mode = -1, no redirection on current command
struct redirInfo *redirInfoPtr;

/**
 * Runs command line loop.
 * General flow:
 *  main loop
 *    -> parseCommand() to turn a single line of command into an array of strings
 *       -> findVar() to replace $
 *       -> redirect() to manage redirection
 *    -> executeCommand() to call the specific funtions
 *       if built-in -> go to their individual functions
 *       if not -> go to launchNewProcess() for execv
 *    -> return to main, determine return code and if need to call addHistory() to add command to history
 *    -> upon exit, all memory are freed either in main for local variables, or in freeStructMem() for structs
 * 
 * For using "history n" to execute a command in history:
 *  in history()
 *    -> parseCommand()
 *    -> executeCommand()
 *    -> when done, free memory and return to executeCommand() then main
 */
int main(int argc, char* argv[]) {

    // 0 if the latest command is successful, -1 otherwise
    // used for return code, is set when a command fails or succeeds to execute
    int error = 0;

    bool interactiveMode = false;

    FILE *inputFile;
    // check if in interactive mode or batch mode
    if(argc == 1) {
        interactiveMode = true;
    }
    else if(argc == 2) {
        inputFile = fopen(argv[1], "r");
        if(inputFile == NULL) {
            fprintf(stderr, "main: open file failed\n");
            exit(-1);
        }
    }
    else {
        exit(-1);
    }

    // initialize history struct
    his = calloc(1, sizeof(struct commandHistory));
    if(his == NULL) {
        fprintf(stderr, "main: calloc failed\n");
        exit(-1);
    }
    his->commands = calloc(HISTORYCAPACITY, sizeof(char *));
    for(int i = 0; i < HISTORYCAPACITY; i++) {
        his->commands[i] = calloc(MAXLENGTH, sizeof(char));
        if(his->commands[i] == NULL)
            fprintf(stderr, "main: calloc failed\n");
    }
    if(his->commands == NULL) {
        fprintf(stderr, "main: calloc failed\n");
        exit(-1);
    }
    his->mostRecentIndex = -1;
    his->capacity = HISTORYCAPACITY;
    his->size = 0;
    
    // give shell variable linked list some space
    shellVarRoot = calloc(1, sizeof(struct shellVariable));
    if(shellVarRoot == NULL) {
        fprintf(stderr, "main: calloc failed\n");
        exit(-1);
    }
  
    // initialize redirection info struct
    redirInfoPtr = calloc(1, sizeof(struct redirInfo));
    redirInfoPtr->mode = -1;
    redirInfoPtr->fname = calloc(MAXLENGTH, sizeof(char));
    if(redirInfoPtr == NULL || redirInfoPtr->fname == NULL) {
        fprintf(stderr, "main: calloc failed\n");
        exit(-1);
    }

    // set initial env variable path
    if(setenv("PATH", "/bin", 1) != 0) {
        fprintf(stderr, "main: setenv failed\n");
        exit(-1);
    }
        
    // for storing current command
    char *input = calloc(MAXLENGTH, sizeof(char));
    if(input == NULL) {
        fprintf(stderr, "main: setenv failed\n");
        exit(-1);
    }

    while(1) {
        
        if(interactiveMode) {

            printf("wsh> ");
            // flush output buffer to make sure "wsh> " gets printed
            fflush(stdout);

            if(fgets(input, MAXLENGTH, stdin) == NULL) {
                // if EOF fgets also returns null, so check for that
                // if EOF, exit with current return code
                if(feof(stdin)) {
                    freeStructMem();
                    free(input);
                    input = NULL;
                    exit(error);
                }
                else {
                    freeStructMem();   
                    free(input);
                    input = NULL;
                    fprintf(stderr, "main: fgets failed\n");
                    exit(-1);
                }
            }
        }
        else {
            if(fgets(input, MAXLENGTH, inputFile) == NULL) {
                if(feof(inputFile)) {
                    freeStructMem();
                    free(input);
                    input = NULL;
                    exit(error);
                }
                else {
                    freeStructMem();   
                    free(input);
                    input = NULL;
                    fprintf(stderr, "main: fgets failed\n");
                    exit(-1);
                }
            }
        }

        // to store an array of string of the current command
        char **current = calloc(MAXARGS, sizeof(char*));
        if(current == NULL) {
            fprintf(stderr, "main: calloc failed\n");
            exit(-1);            
        }
        for(int i = 0; i < MAXARGS; i++) {
            current[i] = calloc(MAXLENGTH, sizeof(char));
            if(current[i] == NULL)
                fprintf(stderr, "main: calloc failed\n");
        }
        // create a copy of original command line as a single string
        // for adding to history
        char *inputCopy = calloc(MAXLENGTH, sizeof(char));
        if(inputCopy == NULL) {
            fprintf(stderr, "main: calloc failed\n");
            exit(-1);
        }

        strcpy(inputCopy, input);
        int argCount = parseCommand(current, input);

        // if parseCommand() returns 0 it failed
        // ignore the rest of the loop
        if(argCount == 0) {
            error = -1;
        }
        else {
            int result = executeCommands(argCount, current);

            // executeCommand() has different return numbers:
            // -2: failed non-built-in commands
            // -1: failed built-in
            //  0: not built-in commands -> add to history
            //  1: built-in command
            //  2: exit was called successfully
            //  3: exit has wrong argument
            // use these return numbers to determine if the command succeeded or not
            // to set the return code
            if(result >= 0 && result < 2) {
                error = 0;
                if(result == 0)
                    addHistory(inputCopy);   
            }
            else if(result < 0){
                error = -1;
                if(result == -2)
                    addHistory(inputCopy);   
            }
            else if(result == 2) {
                // this is when exit is successful
                for(int i = 0; i < MAXARGS; i++) {
                    free(current[i]);
                }
                free(current);
                current = NULL;
                free(input);
                input = NULL;
                free(inputCopy);
                inputCopy = NULL;
                freeStructMem();
                exit(error);
            }
            else if(result == 3) {
                error = -1;
            }
        }

        // free up memory before the next loop
        for(int i = 0; i < MAXARGS; i++) {
            free(current[i]);
        }
        free(current);
        current = NULL;
        free(inputCopy);
        inputCopy = NULL;
    }
}

/**
 * Parse the command into an array of strings, and set redirection if there is one.
 * Input is the command as a single string, and after this method, the array of strings 
 * of the command will be stored in result.
 * Returns the number of arguments in the line of command.
 */
int parseCommand(char** result, char* input) {

    // turn command into an array of strings
    // use a temp array then copy it into result
    char **temp = calloc(MAXARGS, sizeof(char*));

    int argCount = 0;
    temp[argCount] = strtok(input, " \n");

    // first check if first char is # to skip comments
    if(temp[argCount] != NULL && temp[argCount][0] != '#') {
        while(temp[argCount] != NULL) {
            argCount++;
            temp[argCount] = strtok(NULL, " \n");
        }
        int i = 0;

        // replace any $ with its value
        // and copy into result
        while(temp[i] != NULL) {
            strcpy(result[i], temp[i]);
            if(result[i][0] == '$') {
                findVar(result[i]);
            }
            i++;
        }
        // free this element to make the array of strings 
        // to fit the requirement for execv ({"arg1", "arg2", NULL})
        free(result[i]);
        result[i] = NULL;
    }
    free(temp);  
    temp = NULL;

    // if strtok result in a 0 length array, then command failed
    if(argCount == 0) 
        return 0;

    // check redirection
    int length = strlen(result[argCount-1]);
    bool hasRedir = false;

    // check for "&>" and "&>>" then store the file name into the struct
    if(result[argCount-1][0] == '&') {
        if(result[argCount-1][1] == '>') {
            // "&>>"
            if(result[argCount-1][2] == '>') {
                if(result[argCount-1][3] == '>') {
                    fprintf(stderr, "parseCommand: argument wrong format\n");
                    return 0;
                }
                redirInfoPtr->mode = 4;
                char *fn = strtok(result[argCount-1], "&>>");
                if(fn == NULL) {
                    fprintf(stderr, "parseCommand: argument wrong format\n");
                    return 0;
                }
                strcpy(redirInfoPtr->fname, fn);
                hasRedir = true;
            }
            // "&>"
            else {
                redirInfoPtr->mode = 3;
                char *fn = strtok(result[argCount-1], "&>");
                if(fn == NULL) {
                    fprintf(stderr, "parseCommand: argument wrong format\n");
                    return 0;
                }
                strcpy(redirInfoPtr->fname, fn);
                hasRedir = true;
            }
        }
    }

    // check for "<", ">", ">>"
    if(!hasRedir) {
        for(int i = 0; i < length; i++) {
            // "<"
            if(result[argCount-1][i] == '<') {
                // check if there is a second '<' to identify it as an error
                if(i < length - 1 && result[argCount-1][i+1] == '<') {
                    fprintf(stderr, "parseCommand: argument wrong format\n");
                    return 0;
                }
                redirInfoPtr->mode = 0;

                char *fdStr;
                char *fn;
                int fd;
                
                // check if a file descriptor is specified
                // if not, set it to 0
                if(i == 0) {
                    fn = strtok(result[argCount-1], "<");
                    if(fn == NULL) {
                        fprintf(stderr, "parseCommand: argument wrong format\n");
                        return 0;
                    }
                    fd = 0;
                }
                else {
                    fdStr = strtok(result[argCount-1], "<");
                    fn = strtok(NULL, "<");
                    if(fdStr == NULL || fn == NULL) {
                        fprintf(stderr, "parseCommand: argument wrong format\n");
                        return 0;
                    }
                    fd = strtol(fdStr, NULL, 10);
                    if(fd == 0 && fdStr[0] != '0') {
                        fprintf(stderr, "parseCommand: strtol failed\n");
                        return 0;
                    }
                }
                strcpy(redirInfoPtr->fname, fn);
                redirInfoPtr->fileDes = fd;
                // create a copy of the file descriptor for restoring it later
                redirInfoPtr->fdRestore = dup(redirInfoPtr->fileDes);
                hasRedir = true;
                break;
            }
            
            if(result[argCount-1][i] == '>') {
                // ">>"
                if(i < length - 1 && result[argCount-1][i+1] == '>') {
                    // check if there is a third '>' to identify it as an error
                    if(i < length - 2 && result[argCount-1][i+2] == '>') {
                        fprintf(stderr, "parseCommand: argument wrong format\n");
                        return 0;
                    }

                    redirInfoPtr->mode = 2;
                    char *fdStr;
                    char *fn;
                    int fd;

                    // check if a file descriptor is specified
                    // if not, set it to 1
                    if(i == 0) {
                        fn = strtok(result[argCount-1], ">>");
                        if(fn == NULL) {
                            fprintf(stderr, "parseCommand: argument wrong format\n");
                            return 0;
                        }
                        fd = 1;
                    }
                    else {
                        fdStr = strtok(result[argCount-1], ">>");
                        fn = strtok(NULL, ">>");
                        if(fdStr == NULL || fn == NULL) {
                            fprintf(stderr, "parseCommand: argument wrong format\n");
                            return 0;
                        }
                        fd = strtol(fdStr, NULL, 10);
                        if(fd == 0 && fdStr[0] != '0') {
                            fprintf(stderr, "parseCommand: strtol failed\n");
                            return 0;
                        }
                    }

                    strcpy(redirInfoPtr->fname, fn);
                    redirInfoPtr->fileDes = fd;
                    // create a copy of the file descriptor for restoring it later
                    redirInfoPtr->fdRestore = dup(redirInfoPtr->fileDes);
                    hasRedir = true;
                    break;
                }
                // ">"
                else {
                    redirInfoPtr->mode = 1;

                    char *fdStr;
                    char *fn;
                    int fd;

                    if(i == 0) {
                        fn = strtok(result[argCount-1], ">");
                        if(fn == NULL) {
                            fprintf(stderr, "parseCommand: argument wrong format\n");
                            return 0;
                        }
                        fd = 1;
                    }
                    else {
                        fdStr = strtok(result[argCount-1], ">");
                        fn = strtok(NULL, ">");
                        if(fdStr == NULL || fn == NULL) {
                            fprintf(stderr, "parseCommand: argument wrong format\n");
                            return 0;
                        }
                        fd = strtol(fdStr, NULL, 10);
                        if(fd == 0 && fdStr[0] != '0') {
                            fprintf(stderr, "parseCommand: strtol failed\n");
                            return 0;
                        }
                    }
                    strcpy(redirInfoPtr->fname, fn);
                    redirInfoPtr->fileDes = fd;
                    // create a copy of the file descriptor for restoring it later
                    redirInfoPtr->fdRestore = dup(redirInfoPtr->fileDes);
                    hasRedir = true;
                    break;
                }

            }
        }
    }

    //reset mode if current command doesn't have redirection
    if(!hasRedir) {
        redirInfoPtr->mode = -1;
    }
    else {
        // get rid of the redirection part of the argument from the count
        argCount--;
    }

    return argCount;
}

/**
 * Execute the commands by first identifying if it's a built-in command or not.
 * Return -2: failed non-built-in commmands
 *        -1: failed built-in commands
 *         0: not built-in
 *         1: built-in
 *         2: exit successful
 *         3: exit has wrong argument
 */
int executeCommands(int argc, char** command) {

    bool isBuiltIn = false;
    bool isFromPath = false;

    // to catch commands whose only argument is an empty string
    // (resulting from having a $ and a non-existent variable name)
    if(argc == 1 && strlen(command[0]) == 0)
        return -2;

    // scan through the built in command array to find a match
    for(int i = 0; i < builtInCount; i++) {
        // to catch empty command
        if(command[0] == NULL)
            return -1;
        
        if(strcmp(command[0], builtInCommands[i]) == 0) {
            // check if redirection succeeded or not
            int fd = -1;
            if(redirInfoPtr->mode >= 0) {
                fd = redirect();
                if(fd < 0) {
                    fprintf(stderr, "executeCommands: redirect failed\n");
                    return -1;
                }
            }
            // if the command attempted is "exit"
            if(i == 6) {
                if(argc != 1) 
                    return 3;
                else
                    return 2;
            }

            // execute command from a function array
            int rc = (*builtInCommandFunc[i])(argc, command);

            isBuiltIn = true;

            // check if there is redirection, if so, make sure the files are closed
            if(fd != -1 && redirInfoPtr->mode >= 0) {
                dup2(redirInfoPtr->fdRestore, redirInfoPtr->fileDes);
                if(close(fd) < 0) {
                    fprintf(stderr, "executeCommands: close file failed\n");
                    return -1;
                }
                if(fcntl(redirInfoPtr->fdRestore, F_GETFD) != -1 && close(redirInfoPtr->fdRestore) < 0) {
                    fprintf(stderr, "executeCommands: close file failed - 1\n");
                    return -1;
                }
            }

            return rc;
        }
    }

    // full or relative path
    bool found = false;
    char *pathCopy = calloc(MAXLENGTH, sizeof(char));
    if(pathCopy == NULL) {
        fprintf(stderr, "executeCommands: calloc failed\n");
        exit(-1);
    }
    // loop through the first argument to find '/'
    if(!isBuiltIn) {
        int index = 0;
        while(command[0][index] != '\0') {
            if(command[0][index] == '/') {
                isFromPath = true;
                strcpy(pathCopy, command[0]);
                if(access(pathCopy, X_OK) == 0) {
                    found = true;
                    break;
                }
            }
            index ++;
        }
    }

    // for constructing the string for commands from env path to pass into access()
    char* pathFull = calloc(MAXLENGTH, sizeof(char));
    if(pathFull == NULL) {
        fprintf(stderr, "executeCommands: calloc failed\n");
        exit(-1);
    }
        
    // not built in command or from full or relative path
    if(!isBuiltIn && !isFromPath && !found) {

        char* pathEnv = getenv("PATH");

        // search through the paths from the env variable
        char *paths = strtok(pathEnv, ":");
        while (paths != NULL) {
            char *temp = calloc(MAXLENGTH, sizeof(char));
            strcpy(temp, command[0]);
            strcpy(pathFull, paths);
            strcat(pathFull, "/");
            strcat(pathFull, temp);
            if(access(pathFull, X_OK) == 0) {
                found = true;
                free(temp);
                temp = NULL;
                break;
            }
            paths = strtok(NULL, ":");
            free(temp);
            temp = NULL;
        }
    }

    if(found) {
        bool launchProcessFail = false;
        // reconstruct command array if redirection
        if(redirInfoPtr->mode >= 0) {
            char **comWithoutRedir = calloc(MAXARGS+1, sizeof(char*));
            if(comWithoutRedir == NULL) {
                fprintf(stderr, "executeCommands: calloc failed\n");
                exit(-1);
            }
            for(int i = 0; i < argc; i++) {
                comWithoutRedir[i] = calloc(MAXLENGTH, sizeof(char));
                if(comWithoutRedir[i] == NULL) {
                    fprintf(stderr, "executeCommands: calloc failed\n");
                    exit(-1);
                }
                strcpy(comWithoutRedir[i], command[i]);
            }
            // call launchNewProcess() with the correct arguments
            if(isFromPath) {
                if(launchNewProcess(pathCopy, comWithoutRedir) < 0)
                    launchProcessFail = true;
            }
            else {
                if(launchNewProcess(pathFull, comWithoutRedir) < 0)
                    launchProcessFail = true;
            }

            for(int i = 0; i < MAXARGS+1; i++) {
                free(comWithoutRedir[i]);
            }
            free(comWithoutRedir);
            comWithoutRedir = NULL;
                
        }
        else {
            if(isFromPath) {
                if(launchNewProcess(pathCopy, command) < 0)
                    launchProcessFail = true;
            }
            else {
                if(launchNewProcess(pathFull, command) < 0)
                    launchProcessFail = true;             
            }
        }

        free(pathFull);
        pathFull = NULL;
        free(pathCopy);
        pathCopy = NULL;

        // to catch error in launchNewProcess()
        if(launchProcessFail)
            return -2;

        return 0;
    }

    free(pathCopy);
    pathCopy = NULL;
    free(pathFull);
    pathFull = NULL;

    // if not returned before this, command failed
    fprintf(stderr, "executeCommands: command doesn't exist\n");
    return -2;

}

/**
 * Find and replace variable names with their values
 * str passed in is modified.
 */
void findVar(char *str) {

    char *temp = calloc(MAXLENGTH, sizeof(char));

    // get rid of the '$'
    int i = 1;
    while(str[i] != '\0') {
        temp[i - 1] = str[i];
        i++;
    }
    temp[i] = '\0';

    bool found = false;

    // first look if the variable in the env variables
    char *fromEnv = getenv(temp);
    if(fromEnv != NULL) {
        strcpy(str, fromEnv);
        found = true;
    }

    // then look in shell variables
    struct shellVariable* localPtr = shellVarRoot;
    while(!found && localPtr != NULL && localPtr->name != NULL) {

        if(strcmp(temp, localPtr->name) == 0) {
            if(localPtr->value == NULL)
                break;
            strcpy(str, localPtr->value);
            found = true;
        }
        localPtr = localPtr->nextVar;
    }

    // not found, empty string
    if(!found)
        strcpy(str, "");

    free(temp);
}

/**
 * Handle file descriptors and openig files for redirection
 * Return: >=0: succeed, return fd
            -1: fail 
 */
int redirect() {

    // file descriptor of the opened files
    int fd;

    if(redirInfoPtr->mode == 0) {
        fd = open(redirInfoPtr->fname, O_RDONLY);
        if(fd < 0) {
            return -1;
        }
        if (dup2(fd, redirInfoPtr->fileDes) < 0) {
            return -1;
        }
    }
    else if(redirInfoPtr->mode == 1) {
        fd = creat(redirInfoPtr->fname, 00777);
        if(fd < 0) {
            return -1;
        }

        if (dup2(fd, redirInfoPtr->fileDes) < 0) {
            return -1;
        }
    }
    else if(redirInfoPtr->mode == 2) {
        fd = open(redirInfoPtr->fname, O_CREAT | O_APPEND | O_WRONLY, 00777);
        if(fd < 0) {
            return -1;
        }
        if (dup2(fd, redirInfoPtr->fileDes) < 0) {
            return -1;
        }
    }
    else if(redirInfoPtr->mode == 3) {
        fd = creat(redirInfoPtr->fname, 00777);
        if(fd < 0) {
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) < 0) {
            return -1;
        }
    }   
    else if(redirInfoPtr->mode == 4) {
        fd = open(redirInfoPtr->fname, O_CREAT | O_APPEND | O_WRONLY, 00777);
        if(fd < 0) {
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) < 0) {
            return -1;
        }
    }
    else {
        return -1;
    }

    return fd;
}

/**
 * If the command is not a built-in command, create a new process and run it in it.
 * Retrun: 0: succeed
 *        -1: fail
 */
int launchNewProcess(char* path, char *argv[]) {

    int pid = fork();
    int fd = -1;

    // child process
    if(pid == 0) {
        
        // check if there is redirection, if so, make sure the file descriptors are set
        if(redirInfoPtr->mode >= 0) {
            fd = redirect();
            if(fd < 0) {
                fprintf(stderr, "launchNewProcess: redirect failed\n");
                return -1;
            }
        }

        if(execv(path, argv) == -1) {
            fprintf(stderr, "launchNewProcess: execv failed\n");
            return -1;
        }

        // check if there is redirection, if so, make sure the files are closed
        if(redirInfoPtr->mode >= 0) {
            // first restore the file descriptor that was redirected
            dup2(redirInfoPtr->fdRestore, redirInfoPtr->fileDes);
            // close the file
            if(close(fd) < 0) {
                fprintf(stderr, "launchNewProcess: close file failed\n");
                return -1;
            }
            // then close the backup
            if(fcntl(redirInfoPtr->fdRestore, F_GETFD) != -1 && close(redirInfoPtr->fdRestore) < 0) {
                fprintf(stderr, "launchNewProcess: close file failed\n");
                return -1;
            }
        }
    }
    // error
    else if(pid == -1) {
        fprintf(stderr, "launchNewProcess: fork failed\n");
        return -1;
    }
    // parent
    else {
        wait(NULL);
    }
    return 0;
}

/**
 * Add non-built-in commands to history.
 */
void addHistory(char *command) {

    // skip adding if the command is the same as the one just before
    if(his->mostRecentIndex >= 0 && strcmp(his->commands[his->mostRecentIndex], command) == 0)
        return;
    
    // if history is full, wrap around
    if(his->size == his->capacity ) {
        if(his->mostRecentIndex == his->capacity - 1) {
            strcpy(his->commands[0], command);
            his->mostRecentIndex = 0;
        }
        else {
            his->mostRecentIndex++;
            strcpy(his->commands[his->mostRecentIndex], command);
        }
    }
    else {
        if(his->mostRecentIndex < 0 || his->mostRecentIndex == his->capacity - 1) {
            his->mostRecentIndex = 0;
        }
        else {
            his->mostRecentIndex ++;
        }
        if(his->commands[his->mostRecentIndex] == NULL) {
            his->commands[his->mostRecentIndex] = calloc(MAXLENGTH, sizeof(char));
        }
        strcpy(his->commands[his->mostRecentIndex], command);
        his->size++;
    }
}

/**
 * Change the current directory.
 * Return -1: fail
 *         1: success
 */
int cd(int argc, char *argv[]) {
    
    if(argc != 2) {
        fprintf(stderr, "cd: argument wrong format\n");
        return -1;
    }

    if(chdir(argv[1]) != 0) {
        fprintf(stderr, "cd: chdir failed\n");
        return -1;
    }
    return 1;
}

/**
 * Change environment variables.
 * Return -1: fail
 *         1: success
 */
int export(int argc, char *argv[]) {

    // can only have 2 arguments
    if(argc != 2) {
        fprintf(stderr, "export: wrong argument format\n");
        return -1;
    }
        
    // if the second argument doesn't specify variable name
    if(argv[1][0] == '=') {
        fprintf(stderr, "export: wrong argument format\n");
        return -1;
    }

    char *varNameAndValue[MAXLENGTH];

    // make sure there is no more than 1 '='
    int letter = 0;
    int equalSignCount = 0;
    while(argv[1][letter] != '\0') {
        if(argv[1][letter] == '=')
            equalSignCount++;
        letter++;
    }

    if(equalSignCount != 1) {
        fprintf(stderr, "export: wrong argument format\n");
        return -1;
    }

    // get the variable name and value
    int index = 0;
    varNameAndValue[index] = strtok(argv[1], "=");
    while(varNameAndValue[index] != NULL) {
        index++;
        varNameAndValue[index] = strtok(NULL, "=");
    }

    // name and value should only at most each be one
    if (index > 2) {
        fprintf(stderr, "export: wrong argument count\n");
        return -1;
    }

    // check for '$' to find the value of the variable
    if (varNameAndValue[0][0] == '$') {
        fprintf(stderr, "export: wrong argument format\n");
        return -1;
    }
    if (varNameAndValue[1] != NULL && varNameAndValue[1][0] == '$') {
        findVar(varNameAndValue[1]);
    }
    
    // clear variable if empty string
    if(varNameAndValue[1] == NULL) {
        if(setenv(varNameAndValue[0], "", 1) < 0) {
            fprintf(stderr, "export: setenv failed\n");
            return -1;
        }
    }
    else {
        // 1: will overwrite
        if(setenv(varNameAndValue[0], varNameAndValue[1], 1) < 0) {
            fprintf(stderr, "export: setenv failed\n");
            return -1;
        }
    }
    return 1;
}

/**
 * Change local/shell variable
 * Return -1: fail
 *         1: success
 */
int local(int argc, char *argv[]) {

    // must have 2 arguments
    if(argc != 2) {
        fprintf(stderr, "local: wrong argument count\n");
        return -1;
    }
        
    // if the second argument doesn't specify variable name
    if(argv[1][0] == '=') {
        fprintf(stderr, "local: wrong argument format\n");
        return -1;
    }
        
    char *varNameAndValue[MAXLENGTH];

    // make sure there is no more than 1 '='
    int letter = 0;
    int equalSignCount = 0;
    while(argv[1][letter] != '\0') {
        if(argv[1][letter] == '=')
            equalSignCount++;
        letter++;
    }

    if(equalSignCount != 1) {
        fprintf(stderr, "local: wrong argument format\n");
        return -1;
    }
    
    // get the variable name and value
    int index = 0;
    varNameAndValue[index] = strtok(argv[1], "=");
    while(varNameAndValue[index] != NULL) {
        index++;
        varNameAndValue[index] = strtok(NULL, "=");
    }

    // check for '$' to find the value of the variable
    if(varNameAndValue[0][0] == '$') {
        fprintf(stderr, "local: wrong argument format\n");
        return -1;
    }
    if(varNameAndValue[1] != NULL && varNameAndValue[1][0] == '$') {
        findVar(varNameAndValue[1]);
    }

    // put the name and value into the linked list
    // first look if the name is in the list
    struct shellVariable* shellVarPtr = shellVarRoot;
    while(shellVarPtr != NULL) {
        // root has not been filled
        if(shellVarPtr->name == NULL && shellVarPtr->nextVar == NULL) {
            shellVarPtr->name = calloc(MAXLENGTH, sizeof(char));
            shellVarPtr->value = calloc(MAXLENGTH, sizeof(char));
            strcpy(shellVarPtr->name, varNameAndValue[0]);
            if(varNameAndValue[1] == NULL) 
                strcpy(shellVarPtr->value, "");
            else
                strcpy(shellVarPtr->value, varNameAndValue[1]);
            shellVarPtr->nextVar = NULL;
            return 1;
        }
        // variable name matched
        if(strcmp(shellVarPtr->name, varNameAndValue[0]) == 0) {
            // to check if it should be set to empty string
            if(varNameAndValue[1] == NULL) {
                strcpy(shellVarPtr->value, "");
                return 1;
            }
            else {
                strcpy(shellVarPtr->value, varNameAndValue[1]);
                return 1;
            }
        }
        if(shellVarPtr->nextVar == NULL) {
            break;
        }
        shellVarPtr = shellVarPtr->nextVar;
    }
    
    // if reached, it means there is no matching variable, and shellVarPtr is NULL
    // create a new element in the linked list
    shellVarPtr->nextVar = calloc(1, sizeof(struct shellVariable));
    shellVarPtr->nextVar->name = calloc(MAXLENGTH, sizeof(char));
    strcpy(shellVarPtr->nextVar->name, varNameAndValue[0]);
    shellVarPtr->nextVar->value = calloc(MAXLENGTH, sizeof(char));

    if(varNameAndValue[1] == NULL) {
        strcpy(shellVarPtr->nextVar->value, "");
    }
    else {
        strcpy(shellVarPtr->nextVar->value, varNameAndValue[1]);
    }
    return 1;
}

/**
 * Prints shell variables.
 * Return -1: fail
 *         1: success
 */
int vars(int argc, char *argv[]) {
    
    // just to use the parameter
    argv = (void*) argv;

    if(argc != 1) {
        fprintf(stderr, "vars: wrong argument format\n");
        return -1;
    }
        
    struct shellVariable* shellVarPtr = shellVarRoot;
    
    while(shellVarPtr != NULL) {
        if(shellVarPtr->name != NULL)
            printf("%s=%s\n", shellVarPtr->name, shellVarPtr->value);
        shellVarPtr = shellVarPtr->nextVar;
    }
    return 1;
}

/**
 * Three commands involving history: history, history set n, history n.
 * Return -1: fail
 *         1: success
 */
int history(int argc, char *argv[]) {

    int returnVal = 1;
    // print history
    if(argc == 1) {
        
        int index = his->mostRecentIndex;
        for(int i = 1; i <= his->size; i++) {
            printf("%d) %s", i, his->commands[index]);
            index--;
            if(index < 0)
                index = his->size - 1;
        }
    }

    // to modify history size
    else if(argc == 3 && strcmp(argv[1], "set") == 0)  {
        int newSize = strtol(argv[2], NULL, 10);

        if(newSize <= 0) {
            fprintf(stderr, "history set new size <= 0\n");
            return -1;
        }
        else {
            // to see how many elements will be in the new array of commands 
            // count is the index of the most recent command in the new array
            int count;
            if(newSize < his->size)
                count = newSize - 1;
            else
                count = his->size - 1;
               
            // switch around the indices so the most recent history will stay if size got smaller
            // or the order of history will be organzied if size got bigger
            // create a new array and copy things over
            char **tempArray = calloc(newSize, sizeof(char *));
            if(tempArray == NULL) {
                fprintf(stderr, "history set size calloc failed\n");
                return -1;
            }

            for(int i = 0; i < newSize; i++) {
                tempArray[i] = calloc(MAXLENGTH, sizeof(char));
                if(tempArray[i] == NULL) {
                    fprintf(stderr, "history set size calloc failed\n");
                    return -1;
                }
            }
            for(int i = count; i >= 0; i--) {
                strcpy(tempArray[i], his->commands[his->mostRecentIndex]);
                his->mostRecentIndex--;
                if(his->mostRecentIndex < 0)
                    his->mostRecentIndex = his->size - 1;
            }

            // need to free up old array
            for(int i = 0; i < his->capacity; i++) {
                free(his->commands[i]);
            }
            free(his->commands);

            his->commands = tempArray;
            his->capacity = newSize;
            his->size = count + 1; 
            his->mostRecentIndex = count;
        }
    }

    // to execute a command in history
    else if (argc == 2) {
        int indexOfToBeExec = strtol(argv[1], NULL, 10);
        if(indexOfToBeExec <= his->size && indexOfToBeExec > 0) {

            // the array to store the parts of the command is allocated and freed here
            char **toBeExec = calloc(MAXARGS, sizeof(char*));
            if(toBeExec == NULL) {
                fprintf(stderr, "history command calloc failed\n");
                return -1;
            }
            for(int i = 0; i < MAXARGS; i++) {
                toBeExec[i] = calloc(MAXLENGTH, sizeof(char));
                if(toBeExec[i] == NULL) {
                    fprintf(stderr, "history command calloc failed\n");
                    return -1;
                }
            }

            int index = his->mostRecentIndex;
            int count = indexOfToBeExec - 1;
            // to find the command to be executed
            while(count > 0) {
                index--;
                if(index < 0)
                    index = his->size - 1;
                count--;
            }

            // copy the command into a string so the original one in history won't be affected
            char *currCommand = calloc(MAXLENGTH, sizeof(char));
            if(currCommand == NULL) {
                fprintf(stderr, "history command calloc failed\n");
                return -1;
            }
            strcpy(currCommand, his->commands[index]);
            // execute the commands and return the result back to main
            int argCount = parseCommand(toBeExec, currCommand);
            int result = executeCommands(argCount, toBeExec);
            if(result < 0) {
                returnVal = -1;
            } 

            free(currCommand);
            currCommand = NULL;
            for(int i = 0; i < MAXARGS; i++) {
                free(toBeExec[i]);
            }            
            free(toBeExec);
            toBeExec = NULL;
        }
        else {
            returnVal = -1;
        }       
    }
    return returnVal;
}

/**
 * List the files in the current directory
 * Return -1: fail
 *         1: success
 */
int ls(int argc, char *argv[]) {

    // only one argument is allowed
    if(argc > 1) {
        fprintf(stderr, "ls: wrong argument format\n");
        return -1;
    }

    // just to use this argument
    argv = (void*) argv;

    char *currDirPath = calloc(MAXLENGTH, sizeof(char));
    if(currDirPath == NULL) {
        fprintf(stderr, "ls: calloc failed\n");
        return -1;
    }
    
    // get working directory
    getcwd(currDirPath, sizeof(char) * MAXLENGTH);

    // opens a directory stream
    DIR *currDir = opendir(currDirPath);
    if (currDir == NULL) {
        fprintf(stderr, "ls: opendir failed\n");
        return -1;
    }

    // to store all file names
    char **fileNames = calloc(MAXBUFFER, sizeof(char*));
    if(fileNames == NULL) {
        fprintf(stderr, "ls: calloc failed\n");
        return -1;
    }
        
    // a struct of the current entry
    struct dirent *currFile = readdir(currDir);
    int count = 0;

    fileNames[count] = calloc(MAXLENGTH, sizeof(char));
    if(fileNames[count] == NULL) {
        fprintf(stderr, "ls: calloc failed\n");
        return -1;
    }
    // put the entry name into fileNames array
    strcpy(fileNames[count], currFile->d_name);

    // go through the directory until the end
    while(currFile != NULL) {
        count++;
        fileNames[count] = calloc(MAXLENGTH, sizeof(char));
        if(fileNames[count] == NULL)
            return -1;
        strcpy(fileNames[count], currFile->d_name);
        currFile = readdir(currDir);
    }

    // sort the filenames in alphabetical order
    for(int i = count; i >= 0; i--) {
        for(int j = 0; j < i; j++) {
            if(strcmp(fileNames[j], fileNames[j+1]) > 0) {
                char *temp = fileNames[j];
                fileNames[j] = fileNames[j+1];
                fileNames[j+1] = temp;
            }
        }
    }

    // print the names
    for(int i = 0; i <= count; i++) {
        if(fileNames[i][0] != '.') {
            printf("%s\n", fileNames[i]);
        }
        free(fileNames[i]);
    }

    free(currDirPath);
    free(fileNames);
    fileNames = NULL;

    closedir(currDir);
    return 1;
}

/**
 * Free memory of structs
 */
void freeStructMem() {

    for(int i = 0; i < his->capacity; i++) {
        free(his->commands[i]);
    }
    free(his->commands);
    free(his);
    his = NULL;
    
    while(shellVarRoot != NULL) {
        struct shellVariable *temp = shellVarRoot->nextVar;
        free(shellVarRoot->name);
        free(shellVarRoot->value);
        free(shellVarRoot);
        shellVarRoot = temp;
    }

    free(redirInfoPtr->fname);
    free(redirInfoPtr);
    redirInfoPtr = NULL;
}

