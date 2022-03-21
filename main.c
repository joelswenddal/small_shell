/*
* Course: CS 344
* Semester: Fall 2021
* Student: Joel Swenddal
* Assignment: 3 - smallsh
* Description: Program implements a limited version of
* shell in C. The small shell supports the following
* features:
* 
* 1) Provide a prompt for running commands
* 2) Handle blank lines and comments, which are lines beginning with the # character
* 3) Provide expansion for the variable $$
* 4) Execute 3 commands exit, cd, and status via code built into the shell
* 5) Execute other commands by creating new processes using a function from the 
* exec family of functions
* 6) Support input and output redirection
* 7) Support running commands in foreground and background processes
* 8) Implement custom handlers for 2 signals, SIGINT and SIGTSTP
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>


// Constants
#define IN "<"
#define OUT ">"
#define BKGRD "&"
#define COMMENT "#"
#define EXP "$$"
#define MAX_ARGS 512
#define MAX_ARG_LENGTH 2048

// Global variables
bool foregroundOnlyMode = false;

// Struct definitions

/**************************************************
struct: pidNode

Used as node in linked list to keep track of processes
that are allocated to run in the background.
***************************************************/
struct pidNode {

	pid_t* process;
	struct pidNode* next;
};

/**************************************************
struct: pidListHead

Used as head node in linked list to keep track of processes
that are allocated to run in the background.
***************************************************/
struct pidListHead {

	struct pidNode* next;
};

/**************************************************
struct: commandStruct

Key structure to store all the elements that are
included in the shell command. Syntax given for command is:

command [arg1 arg2 ...] [< input_file] [> output_file] [&]

Members are: 1) command string; 2) arguments array;
3) string representing location to redirect input;
4) string representing location to redirect output
5) a bool representing whether the command is meant
run in background mode

***************************************************/
typedef struct {

	char* command;
	char* arguments[MAX_ARGS];
	char* inputRedir;
	char* outputRedir;
	bool bkgrdInd;

} commandStruct;



// Function declarations
char* expandInput(char* inputString);
char* getInput();
commandStruct* processInput(char* inputString);
void printCommandStruct(commandStruct* currentCmdStruct);
void freeCommandStruct(commandStruct* currentCmdStruct);
void exitProcess(char* inputString, commandStruct* currentCmdStruct);
void cdProcess(char* pathString);
void statusProcess(int* lastStatus, int* lastForegroundPid);
void statusBackground(int* childStatus, int* childPid);
void handleSIGTSTP(int signo);
void executeAsChild(commandStruct* currentCmdStruct, int* statusCode,
	int* lastForegroundPid, struct pidListHead* listHead,
	struct sigaction SIGTSTP_action);
void processCheck(struct pidListHead* listHead);



int main(void)
{
	//pointer to command line structure
	commandStruct* commandLine;
	//raw input string from user
	char* input;

	//initialize pointer to head of linked list for tracking pids for 
	// incomplete background processes
	struct pidListHead* pidHead = malloc(sizeof(struct pidListHead));
	pidHead->next = (void*)0;

	//intro header and intro display
	printf("\n");
	fflush(stdout);
	printf("*************************************************\n");
	fflush(stdout);
	printf("$ smallsh\n");
	fflush(stdout);

	int* childStatus = malloc(sizeof(int));
	int* lastForegroundPid = malloc(sizeof(int));
	bool active = true;

	// set up custom signal handling for Ctrl-C /SIGINT: 
	// shell/parent and child processes in background ignore SIGINT
	struct sigaction ignore_action = { {0} }; // { {0} } gets rid of a gcc warning that is a bug
	ignore_action.sa_handler = SIG_IGN;
	sigaction(SIGINT, &ignore_action, NULL);

	//set up custom signal handling for Ctrl-Z /SIGTSTP
	//foreground only mode (see global variable) can be turned on and off
	struct sigaction SIGTSTP_action = { {0} };
	SIGTSTP_action.sa_handler = handleSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	// main loop continues running until the user
	// enters exit command
	while (active)
	{
		input = getInput();
		commandLine = processInput(input);

		// screens out unacceptable command line criteria
		if (commandLine != NULL &&
			strncmp(commandLine->command, COMMENT, 1) != 0 &&
			strcmp(commandLine->command, " ") != 0)

		{
			//for testing
			//printCommandStruct(commandLine);

			//built-in command: cd
			if (strcmp(commandLine->command, "cd") == 0)
			{
				cdProcess(commandLine->arguments[1]);
			}
			//built-in command: status
			else if (strcmp(commandLine->command, "status") == 0)
			{
				statusProcess(childStatus, lastForegroundPid);
			}
			//built-in command: exit
			else if (strcmp(commandLine->command, "exit") == 0)
			{
				exitProcess(input, commandLine);
				break;
			}
			// execute as a child process
			else
			{
				executeAsChild(commandLine, childStatus, lastForegroundPid, pidHead, SIGTSTP_action);
			}
		}

		//checks for completion of background child processes
		//in the tracking linked list and prints them out if completed
		processCheck(pidHead);

		//frees up the memory for command line structures
		free(input);
		if (commandLine != NULL)
		{
			freeCommandStruct(commandLine);
		}
		else
		{
			free(commandLine);
		}
	}

	free(childStatus);
	free(lastForegroundPid);
	printf("\nMain program exiting. Goodbye!\n");
	printf("\n");
	return 0;
}

/**************************************************
Function: processCheck

Function takes a pointer to the head of the
background processes tracking list. Checks the list for
completed background processes. Prints a message
to terminal if any processes are found to be completed.

***************************************************/

void processCheck(struct pidListHead* listHead)
{
	struct pidNode* currentNode;
	struct pidNode* prevNode;
	struct pidNode* tempNode;

	int processStatus;
	pid_t processId;

	//if the list has been populated
	if (listHead->next != (void*)0)
	{
		currentNode = listHead->next;
		prevNode = currentNode;
		tempNode = (void*)0;

		//check for finished background processes
		if ((processId = waitpid(-1, &processStatus, WNOHANG)) > 0)
		{
			// print messages about process status
			statusBackground(&processStatus, &processId);

			//check to see if first node is the match
			if (*currentNode->process == processId)
			{
				if (currentNode->next != (void*)0)
				{
					tempNode = currentNode;
					listHead->next = currentNode->next;
					free(tempNode->process);
					free(tempNode);
				}
			}
			else
				// check other elements in list
			{
				while ((currentNode != (void*)0))
				{
					if (*currentNode->process == processId)
					{
						tempNode = currentNode;
						currentNode = currentNode->next;
						prevNode->next = currentNode;

						free(tempNode->process);
						free(tempNode);
						break;
					}
					else
					{
						prevNode = currentNode;
						currentNode = currentNode->next;
					}
				}
			}
		}
	}
}

/**************************************************
Function: expandInput

Function takes an input string and processes it to identify
substitute the processID for the expansion variable '$$' wherever
it occurs. Dynamically allocates a new string with the expanded
command line. Returns pointer to new array.

References:
Strategy for using strstr() string function based on source:
https://www.tutorialspoint.com/c_standard_library/c_function_strstr.htm

***************************************************/

char* expandInput(char* inputString) {

	//Get count of how many times $$ appears in the string
	//Convert pid to string and get length
	int pidMaxLength = 12;
	long processID = (long)getpid();
	char pidString[pidMaxLength + 1];
	snprintf(pidString, pidMaxLength, "%ld", processID);
	long pidLength = (long)strlen(pidString);
	
	int i = 0;
	char* ptrCur = inputString;
	//char* ptrNext = inputString + 1;
	int symbolCounter = 0;
	
	// iterate through the input string and count instances
	// of the expansion variable -- Working Version
	for (i = 0; inputString[i] != '\0'; i++) 
	{
		//strstr() returns address where substring begins if it is found
		// as the array ptr moves forward, the string field gets smaller
		if (strstr(&inputString[i], EXP) == &inputString[i]) {
			symbolCounter++;
			i++;
		}
	}
	//printf("$$ appeared %d times\n", symbolCounter); //for testing
	//fflush(stdout);

	//int inputStringLength = strlen(inputString);
	int newStringLength = i + (((int)pidLength-2) * symbolCounter);

	char* expandedString = calloc(newStringLength + 1, (int)sizeof(char));

	// go through the inputString and write to the expandedString with pid
	ptrCur = inputString;
	i = 0;
	
	while (*ptrCur)
	{
		//strstr() returns address where substring begins if it is found
		// as the array ptr moves forward, the string field gets smaller
		if (ptrCur == strstr(ptrCur, EXP))
		{
			strcpy(&expandedString[i], pidString);
			i += (int)strlen(pidString);
			ptrCur += 2;
		}
		else 
		{
			expandedString[i] = (int)*ptrCur;
			i++;
			ptrCur++;
		}
	}

	free(inputString);
	return expandedString;
}


/**************************************************
Function: getInput

Function displays command prompt and collects user
input. Returns pointer to character array allocated dynamically
in function. User input string is returned with the variable $$
expanded within the return string. Memory for string allocated
dynamically within function, so must be freed later.

***************************************************/

char* getInput() {

	//Show the command prompt, which is ":"
	
	printf(": ");
	fflush(stdout);

	//shell supports command lines with max length of 2048 chars
	char* inputString = calloc(MAX_ARG_LENGTH + 1, sizeof(char));
	fgets(inputString, MAX_ARG_LENGTH, stdin);

	inputString = expandInput(inputString);

	return inputString;
}


/**************************************************
Function: printCommandStruct

Function takes a pointer to commandStruct and prints out its
members. Used in development.

***************************************************/

void printCommandStruct(commandStruct* currentCmdStruct)
{
	int i = 0;
	if (currentCmdStruct != NULL)
	{
		if (currentCmdStruct->command != NULL)
		{
			printf("Command is: %s \n", currentCmdStruct->command);
			fflush(stdout);
		}
		if (currentCmdStruct->arguments[i] != NULL)
		{
			for (i = 0; i < MAX_ARGS + 1; i++) {
				if (currentCmdStruct->arguments[i] != NULL)
				{
					printf("Argument %d is: %s \n", i, currentCmdStruct->arguments[i]);
					fflush(stdout);
				}
				else
				{
					break;
				}
			}
		}

		if (currentCmdStruct->inputRedir != NULL)
		{
			printf("Redirect input to: %s\n", currentCmdStruct->inputRedir);
			fflush(stdout);
		}
		else
		{
			printf("Redirect input to: Nothing entered\n");
			fflush(stdout);
		}

		if (currentCmdStruct->outputRedir != NULL)
		{
			printf("Redirect output to: %s\n", currentCmdStruct->outputRedir);
			fflush(stdout);
		}
		else
		{
			printf("Redirect output to: Nothing entered\n");
			fflush(stdout);
		}
		printf("Background mode is: %s\n", currentCmdStruct->bkgrdInd ? "true" : "false");
		fflush(stdout);
	}

}


/**************************************************
Function: processInput

Function takes pointer to an input string and processes
it, returning a pointer to a commandStruct whose elements 
are allocated dynamically on the heap. The commandStruct 
that is returned has 5 members representing all 5 categories 
of command line input.

***************************************************/

commandStruct* processInput(char* inputString) {
	
	//allocate dynamic memory
	commandStruct* currentCommand = malloc(sizeof(commandStruct));
	currentCommand->bkgrdInd = false;

	//use strtok_r to separate the input -- first token is command part
	char* tkPtr;
	char* tk = strtok_r(inputString, " \n", &tkPtr);

	// if token is empty (there was no input)
	if (tk == NULL)
	{
		return NULL;
	}
	//populate the new structure -- each element created dynamically
	// command string
	currentCommand->command = calloc(strlen(tk) + 1, sizeof(char));
	strcpy(currentCommand->command, tk);
	
	// first element of argument array is same as command
	currentCommand->arguments[0] = calloc(strlen(currentCommand->command) + 1, sizeof(char));
	strcpy(currentCommand->arguments[0], currentCommand->command);

	//get all arguments from raw input into the arguments array
	int index = 1;   //arguments start at the second element of array
	tk = strtok_r(NULL, " \n", &tkPtr);

	// Loops through input and sends space-delimited tokens to arguments
	// array. Stops when it encounters any of the special characters ( <, >, & )
	while(tk != NULL && (strcmp(tk, IN) != 0) 
		&& (strcmp(tk, OUT) != 0)
		&& (strcmp(tk, BKGRD) != 0))
	{
		currentCommand->arguments[index] = calloc(strlen(tk) + 1, sizeof(char));
		strcpy(currentCommand->arguments[index], tk);
		tk = strtok_r(NULL, " \n", &tkPtr);
		index++;
	}

	// Final element after arguments entered in array should be NULL pointer
	currentCommand->arguments[index] = calloc(1, sizeof(char));
	currentCommand->arguments[index] = NULL;

	// Allocate and assign input and output redirection members
	// if they are present in the input
	do {
		if (tk != NULL && (strcmp(tk, IN) == 0))
		{
			//printf("Current token is: %s\n", token); //for testing
			//fflush(stdout);

			// "<" token present, so save next token as input source
			tk = strtok_r(NULL, " \n", &tkPtr);
			if (tk != NULL) {
				currentCommand->inputRedir = calloc(strlen(tk) + 1, sizeof(char));
				strcpy(currentCommand->inputRedir, tk);
			}
			else { 
				printf("Syntax error after \"<\" \n");
				break; // in case next token is empty
			} 
		}
		// ">" token present, so save next token as output target
		else if (tk != NULL && (strcmp(tk, OUT) == 0))
		{
			tk = strtok_r(NULL, " \n", &tkPtr);
			if (tk != NULL) {
				currentCommand->outputRedir = calloc(strlen(tk) + 1, sizeof(char));
				strcpy(currentCommand->outputRedir, tk);
			}
			else { 
				printf("Syntax error after \">\"\n");
				fflush(stdout);
				break; // in case next token is empty
			} 
		}
		// & token present, so this command should run in background mode
		else if (tk != NULL && (strcmp(tk, BKGRD) == 0))
		{
			currentCommand->bkgrdInd = true;
		}

		tk = strtok_r(NULL, " \n", &tkPtr);

	} while (tk != NULL);

	return currentCommand;
}


/**************************************************
Function: freeCommandStruct

Function takes a pointer to a commandStruct and frees the dynamically
allocated memory for the structure.

***************************************************/

void freeCommandStruct(commandStruct * currentCmdStruct)
{
	if (currentCmdStruct != NULL)
	{
		currentCmdStruct->outputRedir = (void*)0;
		free(currentCmdStruct->command);
	}

	for (int i = 0; i < MAX_ARGS + 1; i++)
	{
		if (currentCmdStruct->arguments[i] != NULL)
		{
			currentCmdStruct->outputRedir = (void*)0;
			free(currentCmdStruct->arguments[i]);
		}

		else
		{
			//free the final allocated pointer to NULL
			//free(currentCmdStruct->arguments[i]);
			break;
		}
	}

	if (currentCmdStruct->inputRedir != NULL)
	{
		currentCmdStruct->inputRedir = (void*)0; //seemed to clear up a segfault in freeing mem
		free(currentCmdStruct->inputRedir);
	}

	if (currentCmdStruct->outputRedir != NULL)
	{
		currentCmdStruct->outputRedir = (void*)0;
		free(currentCmdStruct->outputRedir);
	}
	free(currentCmdStruct);
}


/**************************************************
Function: exitProcess

Function takes a pointer to an inputString and a commandStruct 
and frees the dynamically allocated memory for the structure.

***************************************************/
void exitProcess(char* inputString, commandStruct* currentCmdStruct)
{
	free(inputString); 
	freeCommandStruct(currentCmdStruct);
}

/**************************************************
Function: cdProcess

Function takes string with path to desired directory and
changes the current working directory of the process.

References:
Code adapted from examples at:
https://iq.opengenus.org/chdir-fchdir-getcwd-in-c/
and
Module 4: Exploration on Environment
https://canvas.oregonstate.edu/courses/1830250/pages/
exploration-environment?module_item_id=21468875
***************************************************/

void cdProcess(char* pathString)
{
	char* newDir;
	char cwd[256];

	if (pathString == NULL) 
	{
		newDir = getenv("HOME");
	}
	else
	{
		newDir = pathString;
	}
	
	if (chdir(newDir) != 0)
	{
		perror("chdir() error()");
		fflush(stdout);
	}
	else 
	{
		if (getcwd(cwd, sizeof(cwd)) == NULL)
		{
			perror("getcwd() error");
			fflush(stdout);
		}
		else
		{
			//printf("Current directory is: %s \n", cwd); //for testing
			//fflush(stdout);
		}
	}
}


/**************************************************
Function: statusProcess

Function takes int pointer to status code and last
Foreground process ID. Prints out exit code or
exit signal of last process

References:
Code directly based on examples from
Course Module 4: Monitoring Child Processes
https://canvas.oregonstate.edu/courses/1830250/pages/
exploration-process-api-monitoring-child-processes?module_item_id=21468873
***************************************************/
void statusProcess(int* lastStatus, int* lastForegroundPid)
{
	//printf("Status process function input:  %d\n", *lastStatus); //for testing
	//fflush(stdout);

	if (WIFEXITED(*lastStatus)) {
		//for testing
		//printf("process (%d) exit value %d\n", *lastForegroundPid, WEXITSTATUS(*lastStatus));
		printf("exit value %d\n", WEXITSTATUS(*lastStatus));
		fflush(stdout);
	}
	else {
		//for testing
		//printf("process (%d) terminated by signal %d\n", *lastForegroundPid, WTERMSIG(*lastStatus));
		printf("terminated by signal %d\n", WTERMSIG(*lastStatus));
		fflush(stdout);
	}
}

/**************************************************
Function: statusBackground

Function takes int pointers representing a status of 
a child process and a process ID and prints out messages 
about their resolution to the console. Part of the process of
monitoring and checking background processes.

References:
Code directly based on examples from
Course Module 4: Monitoring Child Processes
https://canvas.oregonstate.edu/courses/1830250/pages/
exploration-process-api-monitoring-child-processes?module_item_id=21468873
***************************************************/

void statusBackground(int* childStatus, int* childPid)
{
	
	if (WIFEXITED(*childStatus)) {
		
		printf("background pid %d is done: exit value %d\n",*childPid, WEXITSTATUS(*childStatus));
		fflush(stdout);
	}
	else {
		
		printf("background pid %d is done: terminated by signal %d\n",*childPid, WTERMSIG(*childStatus));
		fflush(stdout);
	}
}


/**************************************************
Function: handleSIGSTP

 Signal handler function to switch modes for running 
 after receiving SIGTSTP

References:
Course materials: Exploration: Signal handling API:
https://canvas.oregonstate.edu/courses/1830250/pages/
exploration-signal-handling-api?module_item_id=21468881

Signals, Benjamin Brewster, YouTube: https://www.youtube.com/watch?v=VwS3dx3uyiQ
***************************************************/


void handleSIGTSTP(int signo) 
{
	char messageA[51] = "\nEntering foreground-only mode (& is now ignored)\n";
	char messageB[31] = "\nExiting foreground-only mode\n";
	
	if (foregroundOnlyMode == false)
	{
		//switch to forcing processes to foreground only mode (custom mode)
		foregroundOnlyMode = true;
		write(STDOUT_FILENO, messageA, strlen(messageA));
		fflush(stdout);
	}
	else
	{
		//switch to allowing processes to run in background (normal mode)
		foregroundOnlyMode = false;
		write(STDOUT_FILENO, messageB, strlen(messageB));
		fflush(stdout);
	}
}

/**************************************************
Function: executeAsChild

Function takes parameters relevant to execution of a 
child process and exectutes the given command as a child 
process, instead of as a built in command. Updates
status code for the executed child process.

Parameters are:
1) pointer to populated command struct
2) int pointer representing status of a child process
3) int pointer representing last foreground process ID
4) pointer to head of linked list for tracking pids
5) sigaction struc that is loaded with handler function for
SIGTSTP

References:
Based directly on models in Course Module 4 on Process API and
Module 5 Signal Handling API:
https://canvas.oregonstate.edu/courses/1830250/pages/
exploration-process-api-executing-a-new-program?module_item_id=21468874

https://canvas.oregonstate.edu/courses/1830250/pages/
exploration-signal-handling-api?module_item_id=21468881
***************************************************/

void executeAsChild(commandStruct* currentCmdStruct, int* statusCode, int* lastForegroundPid, struct pidListHead* listHead, struct sigaction SIGTSTP_action)

{
	int childStatus;
	int sourceFd;
	int targetFd;
	int resultFd;
	
	// if Foreground Only Mode is on, child processes cannot run in background
	if (foregroundOnlyMode)
	{
		currentCmdStruct->bkgrdInd = false;
	}

	// generate new process
	pid_t spawnPid = fork();

	switch (spawnPid) {
	case -1:
		perror("fork()\n");
		fflush(stdout);
		exit(1);
		break;

	case 0:
		// In the child process
		
		//signal handler defined for child process forcing
		// it to ignore Ctrl-Z/SIGTSTP spec
		SIGTSTP_action.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &SIGTSTP_action, NULL);

		if (foregroundOnlyMode)
		{
			currentCmdStruct->bkgrdInd = false;
		}

		//set so that processes set to run in foreground should have the
		// default behavior (SIG_DFL)
		if (!currentCmdStruct->bkgrdInd)
		{
			struct sigaction default_action = { { 0 } };
			default_action.sa_handler = SIG_DFL;
			sigaction(SIGINT, &default_action, NULL);
		}

		//BRANCH: there is input redirect source OR command will run in background
		if (currentCmdStruct->inputRedir != NULL || currentCmdStruct->bkgrdInd)
		{
			
			if (currentCmdStruct->inputRedir != NULL)
			{
				// open for READ from
				sourceFd = open(currentCmdStruct->inputRedir, O_RDONLY);
			} 
			else
				//open stream will just redirect from nowhere
				//but stay in background
			{
				sourceFd = open("/dev/null", O_RDONLY);
			}
		
			if (sourceFd == -1) {
				printf("cannot open %s for input \n", currentCmdStruct->inputRedir);
				fflush(stdout);
				exit(1);
			}
			//redirects stdin (0) from source file descriptor, which is the source file
			resultFd = dup2(sourceFd, 0);

			if (resultFd == -1) {
				perror("READ error. New file descriptor could not be allocated\n");
				fflush(stdout);
				exit(1);
			}
		}
		//BRANCH: there is output redirect target OR command will run in background
		if (currentCmdStruct->outputRedir != NULL || currentCmdStruct->bkgrdInd)
		{
			if (currentCmdStruct->outputRedir != NULL)
			{
				targetFd = open(currentCmdStruct->outputRedir, O_WRONLY | O_CREAT | O_TRUNC, 0760);
			}

			else
				//open stream will just redirect to nowhere
				//but stay in background - no messages on terminal
			{
				targetFd = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0760);
			}
			
			if (targetFd == -1) {
				printf("cannot open %s for output \n", currentCmdStruct->outputRedir);
				//perror("WRITE error. File could not be opened for output\n");
				fflush(stdout);
				exit(1);
			}

			//redirects stdout (1) to fileDescriptor, which is the target file
			resultFd = dup2(targetFd, 1);

			if (resultFd == -1) {
				perror("WRITE error. New file descriptor could not be allocated\n");
				fflush(stdout);
				exit(1);
			}
		}
		
		// Replace the current program
		execvp(currentCmdStruct->command, currentCmdStruct->arguments);
		// exec only returns if there is an error
		perror(currentCmdStruct->command);
		fflush(stdout);
		exit(2);
		

	default:
		// The parent process

		//CASE: Child runs in foreground
		if (currentCmdStruct->bkgrdInd == false)
		{
			//Parent waits while child finishes
			spawnPid = waitpid(spawnPid, &childStatus, 0);
			//printf("Child process running in foreground \n");    //for testing
			//fflush(stdout);
			
			//statusCode only updates if the child runs as foreground process
			//updates the external variable -- this supports the status function
			*statusCode = childStatus;

			//updates the last foreground process, which here is the child
			*lastForegroundPid = spawnPid;

			// notifies early termination of foreground child process
			if (childStatus == 2)
			{
				printf("terminated by signal %d\n", childStatus);
				fflush(stdout);
			}
			else
			{
				//printf("PARENT(%d): child(%d) terminated, status code: %d\n", getpid(), spawnPid, childStatus);  //for testing
				//fflush(stdout);
			}
		}
		//CASE: Child runs in background
		else
		{
			printf("background pid is %d \n", spawnPid);
			fflush(stdout);
			
			//updates last foreground process, which here is the parent
			*lastForegroundPid = getpid();

			//insert the PID of the child into first spot in the pid list 
			// that tracks currently running background child processes (order doesn't matter)
			struct pidNode* currNode = malloc(sizeof(struct pidNode));
			currNode->process = malloc(sizeof(pid_t));
			*currNode->process = spawnPid;
			currNode->next = (void*)0;
			//struct pidNode* tempNode = (void*)0;
			
			if (listHead->next == (void*)0)
			{
				listHead->next = currNode;
				currNode->next = (void*)0;
			}
			else
			{
				currNode->next = listHead->next;
				listHead->next = currNode;
			}
			

		}
		//usleep(75000); //waits until the child prints it's message that it's in background
		
		//returns control to the parent while child runs in background
		spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);

		//statusProcess(statusCode); //for testing
	}
}




