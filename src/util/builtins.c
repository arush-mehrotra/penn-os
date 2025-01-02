#include "./builtins.h"
#include <stdio.h>
#include <string.h>
#include "./globals.h"

int num_arg(char** args) {
  int i = 0;
  while (args[i] != NULL) {
    i++;
  }
  return i;
}

void* os_sleep(void* arg) {
  char* sleep_arg = ((char**)arg)[1];
  if (sleep_arg == NULL) {
    P_ERRNO = EARG;
    u_error("os_sleep: invalid argument(s) to sleep");
    s_exit();
    return NULL;
  }
  int sleep_time = atoi(sleep_arg);
  if (sleep_time < 0) {
    P_ERRNO = EARG;
    u_error("os_sleep: invalid argument(s) to sleep");
    s_exit();
    return NULL;
  }
  s_sleep(sleep_time);
  return NULL;
}

void* busy(void* arg) {
  while (1) {
    // busy wait
  };
  return NULL;
}

void* ps(void* arg) {
  s_ps();
  s_exit();
  return NULL;
}

void* os_kill(void* arg) {
  char** args = (char**)arg;
  int signal = P_SIGTERM;
  // check first argument
  char* signal_arg = args[1];
  int process_start = 2;
  if (signal_arg[0] == '-') {
    if (strcmp(signal_arg, "-term") == 0) {
      signal = P_SIGTERM;
    } else if (strcmp(signal_arg, "-stop") == 0) {
      signal = P_SIGSTOP;
    } else if (strcmp(signal_arg, "-cont") == 0) {
      signal = P_SIGCONT;
    }
  } else {
    process_start = 1;
  }

  // gather all process ids starting at process_start
  while (args[process_start] != NULL) {
    // check if valid integer
    int pid = atoi(args[process_start]);
    if (pid == 0) {
      P_ERRNO = EARG;
      u_error("os_kill: invalid pid provided to kill");
      s_exit();
      return NULL;
    }
    s_kill(pid, signal);
    process_start++;
  }

  s_exit();

  return NULL;
}

void* cat(void* arg) {
  char** args = (char**)arg;

  pcb* proc = PCBDequeJobSearch(PCBList, currentJob);

  // Check if Filesystem Mounted
  if (fs_fd == -1) {
    P_ERRNO = EFD;
    u_error("cat: filesystem not mounted");
    s_exit();
    return NULL;
  }

  // Argument Casing
  int arg_count = num_arg(args);

  if (arg_count == 1) {
    // Read from STDIN and print to STDOUT

    // If reading from STDIN, stop
    if (proc->process_fdt[0] == STDIN_FILENO && proc->is_background) {
      // Stopping Logic
      s_kill(proc->pid, P_SIGSTOP);
      // Stop thread
      spthread_suspend(proc->curr_thread);
    }

    while (1) {
      char buffer[1024];
      ssize_t num_bytes = s_read(proc->process_fdt[0], 1024, buffer);
      if (num_bytes == -1) {
        s_exit();
        return NULL;
      } else if (num_bytes == 0) {
        break;
      } else {
        buffer[num_bytes] = '\0';
      }

      if (s_write(proc->process_fdt[1], buffer, num_bytes) == -1) {
        s_exit();
        return NULL;
      }
    }
  } else if ((arg_count == 3) &&
             (strcmp(args[1], "-w") == 0 || strcmp(args[1], "-a") == 0)) {
    int fd_write;

    if (strcmp(args[1], "-w") == 0) {
      // Open File (Write)
      fd_write = s_open(args[2], F_WRITE);
    } else if (strcmp(args[1], "-a") == 0) {
      // Open File (Append)
      fd_write = s_open(args[2], F_APPEND);
    }

    if (fd_write == -1) {
      s_exit();
      return NULL;
    }

    // If reading from STDIN, stop
    if (proc->process_fdt[0] == STDIN_FILENO && proc->is_background) {
      // Stopping Logic
      s_kill(proc->pid, P_SIGSTOP);
      // Stop thread
      spthread_suspend(proc->curr_thread);
    }

    while (1) {
      char buffer[1024];
      ssize_t num_bytes = s_read(proc->process_fdt[0], 1024, buffer);
      if (num_bytes == -1) {
        s_exit();
        return NULL;
      } else if (num_bytes == 0) {
        break;
      } else {
        buffer[num_bytes] = '\0';
      }

      if (s_write(fd_write, buffer, num_bytes) == -1) {
        s_exit();
        return NULL;
      }
    }

    // Close Files
    s_close(fd_write);
  } else if (arg_count >= 2 && strcmp(args[arg_count - 2], "-w") != 0 &&
             strcmp(args[arg_count - 2], "-a") != 0) {
    for (int i = 1; i < arg_count; i++) {
      int fd_read;

      fd_read = s_open(args[i], F_READ);

      if (fd_read == -1) {
        s_exit();
        return NULL;
      }
      while (1) {
        char buffer[1024];
        ssize_t num_bytes = s_read(fd_read, 1024, buffer);
        if (num_bytes == -1) {
          s_exit();
          return NULL;
        } else if (num_bytes == 0) {
          break;
        } else {
          buffer[num_bytes] = '\0';
        }

        if (s_write(proc->process_fdt[1], buffer, num_bytes) == -1) {
          s_exit();
          return NULL;
        }
      }

      // Close files

      // Print fd_read
      s_close(fd_read);
    }
  } else if (arg_count >= 2 && strcmp(args[arg_count - 2], "-w") == 0) {
    for (int i = 1; i < arg_count - 2; i++) {
      int fd_read;
      int fd_write;

      fd_read = s_open(args[i], F_READ);
      fd_write = s_open(args[arg_count - 1], F_WRITE);

      if (fd_write == -1 && fd_read == -1) {
        s_exit();
        return NULL;
      }

      if (fd_read == -1) {
        s_close(fd_write);
        s_exit();
        return NULL;
      }

      if (fd_write == -1) {
        s_close(fd_read);
        s_exit();
        return NULL;
      }

      while (1) {
        char buffer[1024];
        ssize_t num_bytes = s_read(fd_read, 1024, buffer);
        if (num_bytes == -1) {
          s_exit();
          return NULL;
        } else if (num_bytes == 0) {
          break;
        } else {
          buffer[num_bytes] = '\0';
        }

        if (s_write(fd_write, buffer, num_bytes) == -1) {
          s_exit();
          return NULL;
        }
      }

      // Close Files
      s_close(fd_read);
      s_close(fd_write);
    }
  } else if (arg_count >= 2 && strcmp(args[arg_count - 2], "-a") == 0) {
    for (int i = 1; i < arg_count - 2; i++) {
      int fd_read;
      int fd_write;

      fd_read = s_open(args[i], F_READ);
      fd_write = s_open(args[arg_count - 1], F_APPEND);

      if (fd_read == -1 && fd_write == -1) {
        s_exit();
        return NULL;
      }

      if (fd_read == -1) {
        s_close(fd_write);
        s_exit();
        return NULL;
      }

      if (fd_write == -1) {
        s_close(fd_read);
        s_exit();
        return NULL;
      }

      while (1) {
        char buffer[1024];
        ssize_t num_bytes = s_read(fd_read, 1024, buffer);
        if (num_bytes == -1) {
          s_exit();
          return NULL;
        } else if (num_bytes == 0) {
          break;
        } else {
          buffer[num_bytes] = '\0';
        }

        if (s_write(fd_write, buffer, num_bytes) == -1) {
          s_exit();
          return NULL;
        }
      }

      // Close Files
      s_close(fd_read);
      s_close(fd_write);
    }
  } else {
    P_ERRNO = EARG;
    u_error("cat: invalid arguments");
    s_exit();
    return NULL;
  }

  s_exit();

  return NULL;
}

void* echo(void* arg) {
  char** args = (char**)arg;

  pcb* proc = PCBDequeJobSearch(PCBList, currentJob);

  // Check if Filesystem Mounted
  if (fs_fd == -1) {
    P_ERRNO = EFD;
    u_error("echo: filesystem not mounted");
    s_exit();
    return NULL;
  }

  // Check # of Arguments
  if (num_arg(args) < 2) {
    P_ERRNO = EARG;
    u_error("echo: invalid number of arguments");
    s_exit();
    return NULL;
  }

  int arg_count = num_arg(args);

  // Print each argument
  for (int i = 1; i < arg_count; i++) {
    s_write(proc->process_fdt[1], args[i], strlen(args[i]));
    if (i != arg_count - 1) {
      s_write(proc->process_fdt[1], " ", 1);
    }
  }

  s_write(proc->process_fdt[1], "\n", 1);

  s_exit();

  return NULL;
}

void* ls(void* arg) {
  char** args = (char**)arg;

  pcb* proc = PCBDequeJobSearch(PCBList, currentJob);

  // Check if Filesystem Mounted
  if (fs_fd == -1) {
    P_ERRNO = EFD;
    u_error("ls: filesystem not mounted");
    s_exit();
    return NULL;
  }

  // Check # of Arguments
  if (num_arg(args) != 1) {
    P_ERRNO = EARG;
    u_error("ls: invalid number of arguments");
    s_exit();
    return NULL;
  }

  // Call s_ls
  if (s_ls(NULL, proc->process_fdt[1]) == -1) {
    s_exit();
    return NULL;
  };

  s_exit();

  return NULL;
}

void* touch(void* arg) {
  char** args = (char**)arg;

  // Check if Filesystem Mounted
  if (fs_fd == -1) {
    P_ERRNO = EFD;
    u_error("touch: filesystem not mounted");
    s_exit();
    return NULL;
  }

  // Check # of Arguments
  int arg_count = num_arg(args);

  // Call s_touch for each file
  for (int i = 1; i < arg_count; i++) {
    if (s_touch(args[i]) == -1) {
      s_exit();
      return NULL;
    };
  }

  s_exit();

  return NULL;
}

void* mv(void* arg) {
  char** args = (char**)arg;

  // Check if Filesystem Mounted
  if (fs_fd == -1) {
    P_ERRNO = EFD;
    u_error("mv: filesystem not mounted");
    s_exit();
    return NULL;
  }

  // Check # of Arguments
  if (num_arg(args) != 3) {
    P_ERRNO = EARG;
    u_error("mv: invalid number of arguments");
    s_exit();
    return NULL;
  }

  // Extract Arguments
  char* source_file = args[1];
  char* dest_file = args[2];

  // Call s_mv
  if (s_mv(source_file, dest_file) == -1) {
    s_exit();
    return NULL;
  };

  s_exit();

  return NULL;
}

void* cp(void* arg) {
  char** args = (char**)arg;

  // Check if Filesystem Mounted
  if (fs_fd == -1) {
    P_ERRNO = EFD;
    u_error("cp: filesystem not mounted");
    s_exit();
    return NULL;
  }

  int arg_count = num_arg(args);

  // Check # of Arguments
  if (arg_count != 3 && arg_count != 4) {
    P_ERRNO = EARG;
    u_error("cp: invalid number of arguments");
    s_exit();
    return NULL;
  }

  // cp SOURCE DEST
  if (arg_count == 3) {
    // Open SOURCE to read
    int fd_read = s_open(args[1], F_READ);
    if (fd_read == -1) {
      s_exit();
      return NULL;
    }

    // Open DEST to write
    int fd_write = s_open(args[2], F_WRITE);
    if (fd_write == -1) {
      s_close(fd_read);
      s_exit();
      return NULL;
    }

    // Read from SOURCE and write to DEST
    while (1) {
      char buffer[1024];
      int num_bytes = s_read(fd_read, 1024, buffer);
      if (num_bytes == -1) {
        s_exit();
        return NULL;
      } else if (num_bytes == 0) {
        break;
      } else {
        buffer[num_bytes] = '\0';
      }

      if (s_write(fd_write, buffer, num_bytes) == -1) {
        s_exit();
        return NULL;
      }
    }

    // Close files
    s_close(fd_read);
    s_close(fd_write);
  }

  // cp -h SOURCE DEST
  if (arg_count == 4 && strcmp(args[1], "-h") == 0) {
    // Open SOURCE to read (from host OS)
    int fd_read = open(args[2], O_RDONLY);
    if (fd_read == -1) {
      s_exit();
      return NULL;
    }

    // Open DEST to write
    int fd_write = s_open(args[3], F_WRITE);
    if (fd_write == -1) {
      close(fd_read);
      s_exit();
      return NULL;
    }

    // Read from SOURCE and write to DEST
    while (1) {
      char buffer[1024];
      ssize_t num_bytes = read(fd_read, buffer, 1024);
      if (num_bytes == -1) {
        s_exit();
        return NULL;
      } else if (num_bytes == 0) {
        break;
      } else {
        buffer[num_bytes] = '\0';
      }

      if (s_write(fd_write, buffer, num_bytes) == -1) {
        s_exit();
        return NULL;
      }
    }

    // Close files
    close(fd_read);
    s_close(fd_write);
  }

  // cp SOURCE -h DEST
  if (arg_count == 4 && strcmp(args[2], "-h") == 0) {
    // Open SOURCE to read
    int fd_read = s_open(args[1], F_READ);
    if (fd_read == -1) {
      s_exit();
      return NULL;
    }

    // Open DEST to write (to host OS)
    int fd_write = open(args[3], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_write == -1) {
      s_close(fd_read);
      s_exit();
      return NULL;
    }

    // Read from SOURCE and write to DEST
    while (1) {
      char buffer[1024];
      ssize_t num_bytes = s_read(fd_read, 1024, buffer);
      if (num_bytes == -1) {
        s_exit();
        return NULL;
      } else if (num_bytes == 0) {
        break;
      } else {
        buffer[num_bytes] = '\0';
      }

      if (write(fd_write, buffer, num_bytes) == -1) {
        s_exit();
        return NULL;
      }
    }

    // Close files
    s_close(fd_read);
    close(fd_write);
  }

  s_exit();

  return NULL;
}

void* rm(void* arg) {
  char** args = (char**)arg;

  // Check if Filesystem Mounted
  if (fs_fd == -1) {
    P_ERRNO = EFD;
    u_error("rm: filesystem not mounted");
    s_exit();
    return NULL;
  }

  // Check # of Arguments
  int arg_count = num_arg(args);

  // Call s_unlink for each file
  for (int i = 1; i < arg_count; i++) {
    s_unlink(args[i]);
  }

  s_exit();

  return NULL;
}

void* chmod(void* arg) {
  char** args = (char**)arg;

  // Check if Filesystem Mounted
  if (fs_fd == -1) {
    P_ERRNO = EFD;
    u_error("chmod: filesystem not mounted");
    s_exit();
    return NULL;
  }

  // Check # of Arguments
  if (num_arg(args) != 3) {
    P_ERRNO = EARG;
    u_error("chmod: invalid number of arguments");
    s_exit();
    return NULL;
  }

  // Get permission type (-/+ rwx)
  char* perm_str = args[1];

  int permission_val = 0;
  int offset = 1;

  // Check if the first character is valid
  if (perm_str[0] != '-' && perm_str[0] != '+') {
    P_ERRNO = EPERM;
    u_error(
        "chmod: invalid permission format: First character must be '+' or "
        "'-'");
    s_exit();
    return NULL;
  }

  // Parse the permission characters
  while (perm_str[offset] != '\0') {
    switch (perm_str[offset]) {
      case 'r':
        permission_val |= 4;  // Read permission
        break;
      case 'w':
        permission_val |= 2;  // Write permission
        break;
      case 'x':
        permission_val |= 1;  // Execute permission
        break;
      default:
        P_ERRNO = EPERM;
        u_error(
            "chmod: invalid permission format: Unknown permission character");
        s_exit();
        return NULL;
    }
    offset++;
  }

  // Call s_chmod
  if (s_chmod(args[2], permission_val, perm_str[0]) == -1) {
    s_exit();
    return NULL;
  };

  s_exit();

  return NULL;
}

/**
 * @brief Spawn a new process for `command` and set its priority to `priority`.
 * 2. Adjust the priority level of an existing process.
 *
 * Example Usage: nice 2 cat f1 f2 f3 (spawns cat with priority 2)
 */
void* u_nice(void* arg) {
  return NULL;
}

/**
 * @brief Adjust the priority level of an existing process.
 *
 * Example Usage: nice_pid 0 123 (sets priority 0 to PID 123)
 */
void* nice_pid(void* arg) {
  char** args = (char**)arg;
  int priority = atoi(args[1]);
  pid_t pid = atoi(args[2]);
  if (s_nice(pid, priority) < 0) {
    u_error("nice");
  }
  return NULL;
}

/**
 * @brief Lists all available commands.
 *
 * Example Usage: man
 */
void* man(void* arg) {
  int output_fd = (int)(intptr_t)arg;

  char message[100];

  sprintf(message, "bg: Resumes a process that is in the background\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "busy: Busy waits indefinitely\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "cat: Concatenate files and print to stdout\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "chmod: Change file permissions for a file\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "cp: Copies a file\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "echo: Echo back an input string provided\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "fg: Bring a process to the foreground and resumes it\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "jobs: Lists all jobs which are running\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "kill: Sends a signal to a process\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "logout: Exits the shell process\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "ls: Lists all working directory files\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "man: Displays the man pages\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "mv: Renames a file\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "nice: Spawns a new process for and set its priority\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "nice_pid: Adjusts priority level of a process\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "orphanify: Creates an orphaned process\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "ps: Display all processes\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "rm: Removes a list of files\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "sleep: Sleeps for x amount of time\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "touch: Creates a new file\n");
  s_write(output_fd, message, strlen(message) + 1);
  sprintf(message, "zombify: Creates a zombied process\n");
  s_write(output_fd, message, strlen(message) + 1);
  return NULL;
}

/**
 * @brief Resumes the most recently stopped job in the background, or the job
 * specified by job_id.
 *
 * Example Usage: bg
 * Example Usage: bg 2 (job_id is 2)
 */
void* bg(void* arg) {
  char* job_arg = ((char**)arg)[1];
  pid_t pid;
  if (job_arg == NULL) {
    pid = -1;
  } else {
    pid = atoi(job_arg);
  }
  int res = s_handle_bg(pid);
  if (res == -1) {
    P_ERRNO = EJOB;
    u_error("bg");
    return NULL;
  }
  return NULL;
}

/**
 * @brief Brings the most recently stopped or background job to the foreground,
 * or the job specified by job_id.
 *
 * Example Usage: fg
 * Example Usage: fg 2 (job_id is 2)
 */
void* fg(void* arg) {
  char* job_arg = ((char**)arg)[1];
  pid_t pid;
  if (job_arg == NULL) {
    pid = -1;
  } else {
    pid = atoi(job_arg);
  }
  int res = s_handle_fg(pid);
  pcb* curr_job = PCBDequeJobSearch(PCBList, currentJob);
  spthread_suspend(curr_job->curr_thread);
  if (res == -1) {
    P_ERRNO = EJOB;
    u_error("bg");
    return NULL;
  }
  return NULL;
}

/**
 * @brief Lists all jobs.
 *
 * Example Usage: jobs
 */
void* jobs(void* arg) {
  if (PCBList == NULL) {
    return NULL;
  }

  int output_fd = (int)(intptr_t)arg;

  PCBDqNode* dq_node = PCBList->front;
  while (dq_node != NULL) {
    // Skip if it is the shell process
    if (dq_node->pcb->pid == 1 || dq_node->pcb->parent_pid != 1) {
      dq_node = dq_node->next;
      continue;
    }
    char* status = (dq_node->pcb)->status == STATUS_RUNNING    ? "running"
                   : (dq_node->pcb)->status == STATUS_STOPPED  ? "stopped"
                   : (dq_node->pcb)->status == STATUS_BLOCKED  ? "blocked"
                   : (dq_node->pcb)->status == STATUS_FINISHED ? "finished"
                                                               : "terminated";
    char message[100];
    char plus = ((dq_node->pcb)->pid == plus_pid) ? '+' : ' ';
    sprintf(message, "[%d]%c %s (%s)\n", (dq_node->pcb)->job_id, plus,
            (dq_node->pcb)->process_name, status);
    s_write(output_fd, message, strlen(message) + 1);
    dq_node = dq_node->next;
  }
  return NULL;
}
/**
 * @brief Exits the shell and shutsdown PennOS.
 *
 * Example Usage: logout
 */
void* logout(void* arg) {
  logged_out = true;
  pcb* proc = PCBDequeJobSearch(PCBList, currentJob);
  spthread_suspend(proc->curr_thread);
  return NULL;
}

/**
 * @brief Helper for zombify.
 */
void* zombie_child(void* arg) {
  s_exit();
  return NULL;
}

/**
 * @brief Used to test zombifying functionality of your kernel.
 *
 * Example Usage: zombify
 */
void* zombify(void* arg) {
  s_spawn(zombie_child, NULL, 0, 1, "zombie_child", true, NULL);
  while (1)
    ;
  s_exit();
  return NULL;
}

/**
 * @brief Helper for orphanify.
 */
void* orphan_child(void* arg) {
  // Please sir,
  // I want some more
  while (1)
    ;
  s_exit();
  return NULL;
}

/**
 * @brief Used to test orphanifying functionality of your kernel.
 *
 * Example Usage: orphanify
 */
void* orphanify(void* arg) {
  s_spawn(orphan_child, NULL, 0, 1, "orphan_child", true, NULL);
  s_exit();
  return NULL;
}
