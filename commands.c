#include "bsh.h"

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

