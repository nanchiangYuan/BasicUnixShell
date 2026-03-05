#include "bsh.h"

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
