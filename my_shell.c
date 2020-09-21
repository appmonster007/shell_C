#include <stdio.h>
#include <string.h>
#include <stdlib.h>			    // exit()
#include <unistd.h>			    // fork(), getpid(), exec()
#include <sys/wait.h>		    // wait()
#include <signal.h>			    // signal()
#include <fcntl.h>	            // close(), open()
#include <sys/stat.h>


#define MAX_NUM_TOKENS 64

char** tokenizeInput(char *input, char* delim)
{
	
	char **cmds = (char**)malloc(sizeof(char*) * MAX_NUM_TOKENS);
	
	for(int j = 0; j < MAX_NUM_TOKENS; j++)
	{
		cmds[j] = NULL;
	}

	int i = 0;
	char *cmd;
	int parallel, outputRedirection;
    
    while((cmd = strsep(&input, delim)) != NULL)
    {
        if 
        (
            strcmp(cmd, "")!=0 && 
            strcmp(cmd, " ")!=0 && 
            strcmp(cmd, "\t")!=0 && 
            strcmp(cmd, "\0")!=0
        )
        {
            cmds[i] = (cmd);
            i++;
        }
    }

	return cmds;
}

char** parseInput(char *input)
{
    return tokenizeInput(input, " ");
}

char* getInput()
{
	size_t sz = 0;
	char* buffer = NULL;
	int len;
	len = getline(&buffer, &sz, stdin);
	buffer[len-1] = 0;
    // printf("%s\n", buffer);

	return buffer;
}

void executeCommand(char *input, int seq)
{
	// This function will fork a new process to execute a command

    char **makeTokens = parseInput(input);
    if(makeTokens[0]!=NULL)
    {
        if (strcmp(makeTokens[0],"exit")==0)
        {
            printf("Exiting shell...\n");
            exit(0);
        }
        else if (strcmp(makeTokens[0], "cd") == 0)
        {
            int i = 1;
            char destination[100];
            memset(destination, 0 ,100);
            while(makeTokens[i+1]!=NULL)
            {
                strcat(destination, makeTokens[i]);
                strcat(destination, " ");
                i++;
            }
            strcat(destination, makeTokens[i]);
            chdir(destination);
        }
        else
        {
            int rc = fork();

            if (rc<0)
            {
                exit(0);
            }
            else if (rc == 0)
            {
                signal(SIGINT, SIG_DFL);
                // Restore the default behavior for SIGINT signal
                if(execvp(makeTokens[0], makeTokens) < 0)
                {
                    printf("Shell: Incorrect command\n");
                    exit(0);
                }
            }
            else
            {
                int rc_wait;
                if (seq)
                {
                    rc_wait = wait(NULL);
                }
            }
        }
    }
}

void executeCommandRedirection(char *input, int seq)
{
    // This function will run a single command 
    // with output redirected to 
    // an output file specificed by user
    char **tokens, **cmdTokens, **outputFile;
    tokens = tokenizeInput(input, ">");
    cmdTokens = tokenizeInput(tokens[0], " ");
    outputFile = tokenizeInput(tokens[1], " ");
    
    int rc = fork();
    if (rc<0)
    {
        exit(0);
    }
    else if (rc == 0)
    {
        signal(SIGINT, SIG_DFL);
        // Restore the default behavior for SIGINT signal
        int defaultSTDOUT = dup(fileno(stdout));
        int fd_new_out = open((outputFile[0]), O_CREAT | O_RDWR | O_APPEND);
        int permissions = fchmod(fd_new_out, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(fd_new_out, fileno(stdout));

        int execution = execvp(cmdTokens[0], cmdTokens);

        dup2(defaultSTDOUT, fileno(stdout));
        close(fd_new_out);
        close(defaultSTDOUT);

        if (execution < 0)
        {
            printf("Shell: Incorrect command\n");
            exit(0);
        }
    }
    else
    {
        int rc_wait = wait(NULL);
    }
    
    free(tokens);
    free(cmdTokens);
    free(outputFile);
}


void executeParallelCommands(char *input)
{
    char **tokens;
    tokens = tokenizeInput(input, "&&");
    int i = 0;
    while (tokens[i]!=NULL)
    {
        if (strstr(tokens[i], ">") != NULL)
        {
            executeCommandRedirection(input, 0);
        }
        else
        {
            executeCommand(tokens[i], 0);
        }
        i++;
    }
    int w_pid, status;
    while ((w_pid = wait(&status)) > 0);
    free(tokens);
}


void executeSequentialCommands(char  *input)
{
    char **tokens;
    tokens = tokenizeInput(input, "##");
    
    int i = 0;
    while (tokens[i]!=NULL)
    {
        if (strstr(tokens[i], "&&") != NULL)
        {
            executeParallelCommands(tokens[i]);
        }
        else if (strstr(tokens[i], ">") != NULL)
        {
            executeCommandRedirection(tokens[i], 1);
        }
        else
        {
            executeCommand(tokens[i], 1);
        }
        i++;
    }
    free(tokens);
}


void excutionController(char* input)
{
    if (strstr(input, "##") != NULL)
    {
        executeSequentialCommands(input);
        // This function is invoked when user wants to run
        // multiple commands sequentially (commands separated by ##)
    }
    else if (strstr(input, "&&") != NULL)
    {
        executeParallelCommands(input);
        // This function is invoked when user wants to run
        // multiple commands in parallel (commands separated by &&)
    }
    else if (strstr(input, ">") != NULL)
    {
        executeCommandRedirection(input, 1);
        // This function is invoked when user wants redirect output
        // of a single command to and output file specificed by user
    }
    else
    {
        executeCommand(input, 1);
        // This function is invoked when user wants to run a single commands
    }
}

char cwd[100]; // currentWorkingDirectory

int main(int argc, char* argv[])
{
    char *input;
	while(1)
	{
        signal(SIGINT, SIG_IGN);
        memset(cwd, 0, sizeof(cwd));
        printf("%s$ ", getcwd(cwd, 100));
        input = getInput();
        excutionController(input);
    }
    return 0;
}

        
