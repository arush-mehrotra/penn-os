#ifndef TERMINAL_HISTORY_H
#define TERMINAL_HISTORY_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HISTORY_SIZE 500  // Maximum number of commands that can be stored

/**
 * @brief Structure to hold the command history of a terminal session.
 *
 * This structure stores commands in a circular buffer, along with the indices
 * for the current command, total number of commands, and the current position
 * in the command history for navigation.
 */
typedef struct {
  char* commands[HISTORY_SIZE];  // Array of pointers to store the commands
  int current;                   // Index of the current command in the buffer
  int size;                      // Total number of commands stored
  int pos;                       // Current navigation position in the history
} TerminalHistory;

// Function prototypes

/**
 * @brief Retrieves the next or previous command from the history based on
 * navigation input.
 *
 * @param history Pointer to the TerminalHistory structure.
 * @param up Boolean flag to determine navigation direction; true for up (older
 * commands).
 * @return Returns the command from the specified direction or NULL if no more
 * commands.
 */
char* get_history(TerminalHistory* history, bool up);

/**
 * @brief Stores a command in the terminal history.
 *
 * @param history Pointer to the TerminalHistory structure.
 * @param command String containing the command to store.
 */
void save_command(TerminalHistory* history, char* command);

/**
 * @brief Reads the command history from a file.
 *
 * @return Returns a pointer to a newly allocated TerminalHistory structure with
 * history loaded from file.
 */
TerminalHistory* read_history_from_file();

/**
 * @brief Frees the memory allocated for the terminal history and its commands.
 *
 * @param history Pointer to the TerminalHistory structure to be freed.
 */
void free_history(TerminalHistory* history);

#endif  // TERMINAL_HISTORY_H