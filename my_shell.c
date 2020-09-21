#include <stdio.h>              // STDIN, STDOUT, STDERR
#include <string.h>             // strstr(), strsep(), strcat()
#include <stdlib.h>			    // exit()
#include <unistd.h>			    // fork(), getpid(), exec()
#include <sys/wait.h>           // wait()
#include <signal.h>			    // signal()
#include <fcntl.h>              // close(), open()
#include <sys/stat.h>           // fchmod(), chmod()


#define MAX_NUM_TOKENS 64

char** tokenizeInput(char *input, char* delim)
{
	// this functions takes a string as "input" and splits/separates/tokenizes it
    // into multiple part depending on delimiter "delim"
    // and returns the array cotaining all separate strings or tokens
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
        // this check may not be necessary, but exists for precaution
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
    // tokenize the input on basis of " "
    return tokenizeInput(input, " ");
}

char* getInput()
{
    // get input from interaction using STDIN
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
        // to exit shell
        if (strcmp(makeTokens[0],"exit") == 0)
        {
            printf("Exiting shell...\n");
            exit(0);
        }
        // to change Current Working Directory
        else if (strcmp(makeTokens[0], "cd") == 0)
        {
            // concat strings to form name/path of file for correct execution
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
        // to execute given input command
        else
        {
            // create a new child process for execution of given command
            int rc = fork();

            if (rc<0)
            {
                exit(0);
            }
            else if (rc == 0)
            {
                
                // Restore the default behavior for SIGINT signal
                signal(SIGINT, SIG_DFL);
                if(execvp(makeTokens[0], makeTokens) < 0)
                {
                    printf("Shell: Incorrect command\n");
                    exit(0);
                }
            }
            else
            {
                // parent must wait if command is executed sequentially
                // seq = 1 : sequential command
                // seq = 0 : parallel command
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
    
    // split input command into two tokens
    // 1. command to execute
    // 2. file for redirected output
    char **tokens, **cmdTokens, **outputFile;
    tokens = tokenizeInput(input, ">");
    cmdTokens = tokenizeInput(tokens[0], " ");
    outputFile = tokenizeInput(tokens[1], " ");
    
    // create a new child process for execution
    int rc = fork();
    if (rc<0)
    {
        exit(0);
    }
    else if (rc == 0)
    {
        // Restore the default behavior for SIGINT signal
        signal(SIGINT, SIG_DFL);
        
        // Output redirected to given file/path
        int defaultSTDOUT = dup(fileno(stdout));
        int fd_new_out = open((outputFile[0]), O_CREAT | O_RDWR | O_APPEND);
        
        // set permission (644) to file/path
        int permissions = fchmod(fd_new_out, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        
        dup2(fd_new_out, fileno(stdout));

        int execution = execvp(cmdTokens[0], cmdTokens);

        // Restore the default stdout
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
    
    // free allocated memory
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
            // This function is invoked when user wants redirect output
            // of a command to and output file specificed by user
            executeCommandRedirection(input, 0);
        }
        else
        {
            // This function is invoked when user wants to run parallel commands
            executeCommand(tokens[i], 0);
        }
        i++;
    }
    
    // To wait for all child processes, which were executed parallel, to finish
    int w_pid, status;
    while ((w_pid = wait(&status)) > 0);
    
    // free allocated memory
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
            // This function is invoked when user wants to run
            // multiple commands in parallel (commands separated by &&)
            // followed by sequential commands
            executeParallelCommands(tokens[i]);
        }
        else if (strstr(tokens[i], ">") != NULL)
        {
            // This function is invoked when user wants redirect output
            // of a command to and output file specificed by user
            executeCommandRedirection(tokens[i], 1);
        }
        else
        {
            // This function is invoked when user wants to run commands in sequential
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
        // This function is invoked when user wants to run
        // multiple commands sequentially (commands separated by ##)
        executeSequentialCommands(input);
    }
    else if (strstr(input, "&&") != NULL)
    {
        // This function is invoked when user wants to run
        // multiple commands in parallel (commands separated by &&)
        executeParallelCommands(input);
    }
    else if (strstr(input, ">") != NULL)
    {
        // This function is invoked when user wants redirect output
        // of a single command to and output file specificed by user
        executeCommandRedirection(input, 1);
    }
    else
    {
        // This function is invoked when user wants to run a single commands
        executeCommand(input, 1);
    }
}

char cwd[100]; // currentWorkingDirectory

int main(int argc, char* argv[])
{
    char *input;
	while(1)
	{
        // Ignore the SIGINT (interrupt signal)
        signal(SIGINT, SIG_IGN);
        
        memset(cwd, 0, sizeof(cwd));
        
        // Print the prompt in format - currentWorkingDirectory$ 
        printf("%s$ ", getcwd(cwd, 100));
        
        // accept input with 'getline()'
        input = getInput();
        
        // Controls the execution of input
        excutionController(input);
    }
    return 0;
}

        
