#include "shell.h"

#define MAX_LINE_LENGTH 1024

const int promptSize = 2;

int input_file;
int output_file;

// Define a structure to map strings to function pointers
typedef struct {
  char* name;
  void* (*function)(void*);
} BuiltinMap;

// Create a static array of the structure
static BuiltinMap function_map[] = {
    {"sleep", os_sleep},
    {"busy", busy},
    {"ps", ps},
    {"kill", os_kill},
    {"cat", cat},
    {"echo", echo},
    {"ls", ls},
    {"touch", touch},
    {"mv", mv},
    {"cp", cp},
    {"rm", rm},
    {"chmod", chmod},
    {"fg", fg},
    {"bg", bg},
    {"hang", hang},
    {"nohang", nohang},
    {"recur", recur},
    {"nice", u_nice},
    {"nice_pid", nice_pid},
    {"zombify", zombify},
    {"orphanify", orphanify},
    {"jobs", jobs},
    {"man", man},
    {"logout", logout},
    {NULL, NULL}  // Terminator
};

bool handle_io_setup(struct parsed_command* parsed,
                     int* in_file,
                     int* out_file) {
  if (parsed->stdin_file != NULL) {
    *in_file = s_open(parsed->stdin_file, F_READ);
    if (*in_file < 0) {
      P_ERRNO = EIO;
      u_error(parsed->stdin_file);
      return false;
    }
  } else {
    *in_file = STDIN_FILENO;
  }

  if (parsed->stdout_file != NULL) {
    if (parsed->is_file_append) {
      *out_file = s_open(parsed->stdout_file, F_APPEND);
    } else {
      *out_file = s_open(parsed->stdout_file, F_WRITE);
    }
    if (*out_file < 0) {
      P_ERRNO = EIO;
      u_error(parsed->stdout_file);
      if (*in_file != STDIN_FILENO) {
        s_close(*in_file);
      }
      return false;
    }
  } else {
    *out_file = STDOUT_FILENO;
  }
  return true;
}

bool is_shell_builtin(struct parsed_command* parsed) {
  if (strcmp(parsed->commands[0][0], "nice_pid") == 0) {
    nice_pid(parsed->commands[0]);
    return true;
  } else if (strcmp(parsed->commands[0][0], "man") == 0) {
    // cast output_file and pass in to man
    man((void*)(intptr_t)output_file);
    return true;
  } else if (strcmp(parsed->commands[0][0], "bg") == 0) {
    bg(parsed->commands[0]);
    return true;
  } else if (strcmp(parsed->commands[0][0], "fg") == 0) {
    fg(parsed->commands[0]);
    return true;
  } else if (strcmp(parsed->commands[0][0], "jobs") == 0) {
    // cast output_file and pass in to man
    jobs((void*)(intptr_t)output_file);
    return true;
  } else if (strcmp(parsed->commands[0][0], "logout") == 0) {
    free(parsed);
    logout(NULL);
    return true;
  }
  return false;
}

void reap_jobs() {
  int res = 0;
  int status = 0;
  while ((res = s_waitpid(-1, &status, true) > 0)) {
  }
}
// Function to match the string and set the function pointer and process name
void builtin_matcher(char* str,
                     void* (**os_proc_func)(void*),
                     char** process_name) {
  for (int i = 0; function_map[i].name != NULL; i++) {
    if (strcmp(str, function_map[i].name) == 0) {
      *os_proc_func = function_map[i].function;
      *process_name = function_map[i].name;
      return;
    }
  }
  // If no match is found
  *os_proc_func = NULL;
  *process_name = NULL;
}

// void read_command(char* cmd, ssize_t* read_res) {
//   if (s_write(STDERR_FILENO, PROMPT, PROMPT_SIZE) == -1) {
//     s_exit();
//   }
//   *read_res = read(STDIN_FILENO, cmd, MAX_LENGTH);
//   if (*read_res < 0) {
//     s_write(STDERR_FILENO, "\n", 1);
//     s_exit();
//   } else if (*read_res == 0) {
//     logout(NULL);
//   }
//   cmd[*read_res / sizeof(char)] = '\0';
// }

void read_command(char* cmd, ssize_t* read_res) {
  if (s_write(STDERR_FILENO, PROMPT, promptSize) == -1) {
    s_exit();
  }

  struct termios orig_termios, new_termios;
  tcgetattr(STDIN_FILENO, &orig_termios);
  new_termios = orig_termios;
  new_termios.c_lflag &= ~(ICANON | ECHO);  // Clear ICANON and ECHO.
  new_termios.c_cc[VMIN] = 1;
  new_termios.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);

  int index = 0;
  char ch;
  while (true) {
    ssize_t nread = read(STDIN_FILENO, &ch, 1);
    // Print out &ch
    if (nread == 0) {  // EOF detected, user pressed Ctrl-D
      logout(NULL);    // Call logout function to terminate the shell
      tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
      break;  // Break out of the loop after handling EOF
    } else if (nread == 1 && ch == '\n') {
      cmd[index] = '\0';
      *read_res = index;
      s_write(STDOUT_FILENO, &ch, 1);  // Echo newline
      tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
      break;                    // Newline detected, end of command input
    } else if (ch == '\033') {  // Escape sequence for arrow keys
      char seq[3];
      if (read(STDIN_FILENO, &seq[0], 1) == 0 || seq[0] != '[')
        continue;
      if (read(STDIN_FILENO, &seq[1], 1) == 0)
        continue;
      if (seq[1] == 'A') {  // Up arrow
        char* up = get_history(curr_history, true);
        if (up == NULL) {
          continue;
        }
        strcpy(cmd, up);
        index = strlen(cmd);
        char* clear = "\033[2K\r";
        s_write(STDOUT_FILENO, clear, strlen(clear));
        s_write(STDOUT_FILENO, PROMPT, promptSize);
        s_write(STDOUT_FILENO, up, strlen(up));
        continue;
      } else if (seq[1] == 'B') {  // Down arrow
        char* down = get_history(curr_history, false);
        if (down == NULL) {
          continue;
        }
        strcpy(cmd, down);
        index = strlen(cmd);
        char* clear = "\033[2K\r";
        s_write(STDOUT_FILENO, clear, strlen(clear));
        s_write(STDOUT_FILENO, PROMPT, promptSize);
        s_write(STDOUT_FILENO, down, strlen(down));
        continue;
      } else {
        continue;
      }
    }
    if (nread == 1) {
      // Check for EOF
      if (ch == 4) {
        logout(NULL);
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        break;
      }
      if (ch == 127 || ch == '\b') {  // Handle backspace
        if (index > 0) {
          index--;
          s_write(STDOUT_FILENO, "\b \b",
                  3);  // Erase last character on terminal
        }
      } else {
        cmd[index++] = ch;               // Store normal characters
        s_write(STDOUT_FILENO, &ch, 1);  // Echo back the character
      }
    }
  }
  cmd[index] = '\0';
  *read_res = index;
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);  // Reset terminal settings
}

void* shell(void* arg) {
  // Run os loop
  ssize_t read_res = 0;
  while (true) {
    char cmd[MAX_LENGTH];
    // Read command into cmd
    read_command(cmd, &read_res);
    save_command(curr_history, cmd);  // Save the command to history
    // reap finished/terminated jobs
    reap_jobs();
    struct parsed_command* parsed = NULL;
    int parse_res = parse_command(cmd, &parsed);
    if (parse_res != 0) {
      P_ERRNO = EPARSE;
      u_error("shell");
      continue;
    }
    if (read_res < 0) {
      P_ERRNO = ECMD;
      u_error("shell");
      free(parsed);
      parsed = NULL;
      continue;
    }
    // Check if commands equal to 0
    if (parsed->num_commands == 0) {
      free(parsed);
      parsed = NULL;
      continue;
    }

    if (!handle_io_setup(parsed, &input_file, &output_file)) {
      free(parsed);
      parsed = NULL;
      continue;
    }

    void* (*os_proc_func)(void*);
    char* process_name;
    builtin_matcher(parsed->commands[0][0], &os_proc_func, &process_name);
    pid_t child;

    // Did not find a match with one of the built-ins
    if (os_proc_func == NULL) {
      // Scripting
      int read_bytes;
      char buffer[1024];
      memset(buffer, 0, sizeof(buffer));

      // Try opening the script file to read from
      int fd = s_open(parsed->commands[0][0], F_READ);
      if (fd == -1) {
        P_ERRNO = EIO;
        u_error("Error reading script file");
        free(parsed);
        parsed = NULL;
        if (input_file != STDIN_FILENO) {
          s_close(input_file);
        }
        if (output_file != STDOUT_FILENO) {
          s_close(output_file);
        }
        continue;
      }

      // Check permissions of the file
      int perm_res = s_findperm(parsed->commands[0][0]);

      if (perm_res == 0 || perm_res == 2 || perm_res == 4 || perm_res == 6) {
        P_ERRNO = EPERM;
        u_error("Error opening script file");
        s_close(fd);
        free(parsed);
        parsed = NULL;
        if (input_file != STDIN_FILENO) {
          s_close(input_file);
        }
        if (output_file != STDOUT_FILENO) {
          s_close(output_file);
        }
        continue;
      }

      // Read entire file into command
      if ((read_bytes = s_read(fd, 1024, buffer)) != -1) {
        // Iterate through command and separate by newlines
        char* individual_command = strtok(buffer, "\n");
        while (individual_command != NULL) {
          // Create a new parsed command (create new buffer of commnand)
          struct parsed_command* script_parsed = NULL;
          int script_parse_result =
              parse_command(individual_command, &script_parsed);
          if (script_parse_result != 0) {
            P_ERRNO = EPARSE;
            u_error("Error parsing command");
            // Close the file descriptors
            s_close(fd);
            if (input_file != STDIN_FILENO) {
              s_close(input_file);
            }
            if (output_file != STDOUT_FILENO) {
              s_close(output_file);
            }
            continue;
          }
          if (script_parsed->num_commands == 0) {
            free(script_parsed);
            script_parsed = NULL;
            // Close the file descriptors
            s_close(fd);
            if (input_file != STDIN_FILENO) {
              s_close(input_file);
            }
            if (output_file != STDOUT_FILENO) {
              s_close(output_file);
            }
            continue;
          }

          void* (*os_proc_func_script)(void*);
          char* process_name_script;
          builtin_matcher(script_parsed->commands[0][0], &os_proc_func_script,
                          &process_name_script);

          child = s_spawn(os_proc_func_script, script_parsed->commands[0],
                          input_file, output_file, process_name_script, false,
                          script_parsed);

          int child_status = -1;
          if (s_waitpid(child, &child_status, false) < 0) {
            printf("FINISHED WAITING ON CHILD\n");

            free(script_parsed);
            script_parsed = NULL;

            free(parsed);
            parsed = NULL;

            s_exit();
          }

          // memset(buffer, 0, sizeof(buffer));
          individual_command = strtok(NULL, "\n");
        }

        free(parsed);
        parsed = NULL;

        s_close(fd);
        // Close the file descriptors
        if (input_file != STDIN_FILENO) {
          s_close(input_file);
        }
        if (output_file != STDOUT_FILENO) {
          s_close(output_file);
        }
        continue;
      } else {
        P_ERRNO = EIO;
        u_error("Error reading script file");
        s_close(fd);
        free(parsed);
        parsed = NULL;
        // Close the file descriptors
        if (input_file != STDIN_FILENO) {
          s_close(input_file);
        }
        if (output_file != STDOUT_FILENO) {
          s_close(output_file);
        }
        continue;
      }
    }

    // handler for background foreground
    if (is_shell_builtin(parsed)) {
      free(parsed);
      parsed = NULL;

      if (input_file != STDIN_FILENO) {
        s_close(input_file);
      }
      if (output_file != STDOUT_FILENO) {
        s_close(output_file);
      }

      continue;
    }
    if (strcmp(parsed->commands[0][0], "nice") == 0) {
      // The new name and function is the third item in the command
      builtin_matcher(parsed->commands[0][2], &os_proc_func, &process_name);
      if (os_proc_func == NULL) {
        P_ERRNO = ECMD;
        u_error("nice");
        free(parsed);
        parsed = NULL;

        if (input_file != STDIN_FILENO) {
          s_close(input_file);
        }
        if (output_file != STDOUT_FILENO) {
          s_close(output_file);
        }

        continue;
      }

      // Find number of total words in the command
      int num_elements = 0;
      char* command = parsed->commands[0][num_elements];
      while (command != NULL) {
        num_elements++;
        command = parsed->commands[0][num_elements];
      }

      // First two things are included
      char* actual_command[num_elements - 1];
      for (int i = 2; i < num_elements; i++) {
        actual_command[i - 2] = parsed->commands[0][i];
      }
      actual_command[num_elements - 2] = NULL;

      // Create the thread
      pid_t child =
          s_spawn(os_proc_func, actual_command, input_file, output_file,
                  process_name, parsed->is_background, parsed);
      int child_status = -1;

      // Assign the priority
      int priority = atoi(parsed->commands[0][1]);
      if (s_nice(child, priority) < 0) {
        free(parsed);
        parsed = NULL;

        u_error("Error setting priority level");

        if (input_file != STDIN_FILENO) {
          s_close(input_file);
        }
        if (output_file != STDOUT_FILENO) {
          s_close(output_file);
        }

        continue;
      }

      // Wait on it if necessary
      if (!parsed->is_background) {
        if (s_waitpid(child, &child_status, false) < 0) {
          free(parsed);
          parsed = NULL;

          s_exit();
        }
      }
    } else {
      pid_t child =
          s_spawn(os_proc_func, parsed->commands[0], input_file, output_file,
                  process_name, parsed->is_background, parsed);

      int child_status = -1;

      // only wait if the job is in the foreground
      if (!parsed->is_background) {
        if (s_waitpid(child, &child_status, false) < 0) {
          free(parsed);
          parsed = NULL;

          s_exit();
        }
      }
    }

    // Close the file descriptors
    if (input_file != STDIN_FILENO) {
      s_close(input_file);
    }
    if (output_file != STDOUT_FILENO) {
      s_close(output_file);
    }
  }
}