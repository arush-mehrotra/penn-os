#include "./terminal_history.h"

const char* HISTORY_FILE = "./log/history.txt";

char* get_history(TerminalHistory* history, bool up) {
  // if up is true, move back in history (unless current + pos == 0, then return
  // NULL since we are at the beginning of history) if up is false, move forward
  // in history (unless current + pos == size, then return NULL since we are at
  // the end of history) return the command at the new position and update the
  // pos
  if (history->size == 0) {
    return NULL;
  }

  if (up) {
    if ((history->current + history->pos) == 0) {
      return NULL;  // we are at the beginning of history
    }
    history->pos--;
  } else {
    if ((history->current + history->pos) >= history->size) {
      return NULL;  // we are at the end of history
    }
    history->pos++;
  }
  return history->commands[history->current + history->pos];
}

// take in command and write command to a new line in text file with path
// "filename" update pos, current, and size in the history struct
void save_command(TerminalHistory* history, char* command) {
  FILE* file = fopen(HISTORY_FILE, "a");  // Open file for appending
  if (file == NULL) {
    perror("Failed to open file");
    return;
  }
  fprintf(file, "%s\n", command);  // Append the command to the file
  fclose(file);

  // Save command to history
  if (history->current >= HISTORY_SIZE - 1) {
    perror(
        "History is full! Not saving any commands locally (saved to storage "
        "tho)\n");
    return;
  }
  history->size++;
  history->current++;
  history->commands[history->current] = strdup(command);
  history->pos = 1;  // Reset the position for navigation
}

// read in a file with path "filename" and populate a TerminalHistory struct
// with the commands each command should be stored in the commands array in the
// order they were read the current index should be set to index of the last
// command read (for n commands, the index should be n-1) the size should be set
// to the number of commands read (for n commands, the size should be n) the pos
// should be set to 0
TerminalHistory* read_history_from_file() {
  TerminalHistory* history = malloc(sizeof(TerminalHistory));
  if (history == NULL) {
    perror("Failed to allocate memory for history");
    return NULL;
  }
  history->current = 0;
  history->size = 0;
  history->pos = 1;

  for (int i = 0; i < HISTORY_SIZE; i++) {
    history->commands[i] = NULL;  // Initialize each pointer to NULL
  }

  FILE* file = fopen(HISTORY_FILE, "r");
  if (!file) {
    perror("Failed to open file");
    return history;
  }

  char* line = NULL;
  size_t len = 0;
  ssize_t read;
  while ((read = getline(&line, &len, file)) != -1) {
    line[strcspn(line, "\n")] = 0;  // Remove newline
    history->commands[history->size] = strdup(line);
    history->size++;
    if (history->size >= HISTORY_SIZE)
      break;  // Prevent overflow
  }
  fclose(file);
  free(line);

  history->current = history->size - 1;
  return history;
}

void free_history(TerminalHistory* history) {
  // free all dynamically allocated memory
  if (history == NULL) {
    return;  // Check if the history pointer is null to avoid dereferencing null
  }
  // Loop through the commands array and free each dynamically allocated string
  for (int i = 0; i < history->size; i++) {
    if (history->commands[i] != NULL) {
      free(history->commands[i]);
      history->commands[i] =
          NULL;  // Set the pointer to NULL after freeing to avoid double free
    }
  }
  free(history);
}
