#include "bsh.h"

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