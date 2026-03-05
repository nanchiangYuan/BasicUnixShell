#include "bsh.h"

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

