Course: CS 344
Semester: Fall 2021
Student: Joel Swenddal
Assignment: 3 - smallsh
Description: Program implements a limited version of
shell using C. The small shell supports the following
features:

1) Provide a prompt for running commands
2) Handle blank lines and comments, which are lines beginning with the # character
3) Provide expansion for the variable $$
4) Execute 3 commands exit, cd, and status via code built into the shell
5) Execute other commands by creating new processes using a function from the 
exec family of functions
6) Support input and output redirection
7) Support running commands in foreground and background processes
8) Implement custom handlers for 2 signals, SIGINT and SIGTSTP


Project file contents:

1) main.c - All source code is contained in this .c file

2) README.txt - Contains instructions to compile

------------------------------------------------------------------------

To compile the program:
Use gcc with gnu99 standard --

1) Unzip file to a directory; navigate to project directory

2) In project directory, enter: gcc -std=gnu99 -o smallsh main.c

3) To run the program, enter ./smallsh