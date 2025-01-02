#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "../util/builtins.h"
#include "../util/globals.h"
#include "../util/os_errors.h"
#include "../util/parser.h"
#include "stress.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#define MAX_LENGTH 4096
#define PROMPT_SIZE 2

#ifndef PROMPT
#define PROMPT "$ "
#endif

/**
 * @brief Manages the input/output redirection for a command.
 *
 * @param parsed Parsed command struct to check for redirection
 * @param in_file Pointer to the file descriptor for input
 * @param out_file Pointer to the file descriptor for output
 * @return true If the redirection was successful
 * @return false If the redirection was unsuccessful
 */
bool handle_io_setup(struct parsed_command* parsed,
                     int* in_file,
                     int* out_file);

/**
 * @brief Matches a command to a builtin function.
 *
 * @param str String to match
 * @param os_proc_func Function pointer to the matched function
 * @param process_name Name of the matched function
 */
void builtin_matcher(char* str,
                     void* (**os_proc_func)(void*),
                     char** process_name);

/**
 * @brief Prints out shell prompt and reads user input into a buffer
 *
 * @param cmd
 * @param read_res Pointer to the result of the read indicating the number of
 * bytes read
 */
void read_command(char* cmd, ssize_t* read_res);

/**
 * @brief Function to be executed by the shell thread.
 *
 */
void*(shell)(void*);