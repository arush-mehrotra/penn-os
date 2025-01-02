#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "fat/fat_helper.h"
#include "util/globals.h"
#include "util/parser.h"

#include "util/PCBDeque.h"
#include "util/PIDDeque.h"
#include "util/os_errors.h"

// MACROS
#define PROMPT "pennfat# "
#define MAX_LINE_LEN 4096

// Global Variables
int fs_fd = -1;          // File Descriptor for FAT
uint16_t* fat = NULL;    // FAT
global_fdt g_fdt[1024];  // Global File Descriptor Table
int g_counter = 0;
pid_t fgJob = 0;
bool logged_out = false;
pid_t plus_pid = -1;
pid_t currentJob = 0;
int num_bg_jobs = 0;
int P_ERRNO = 0;

// Declared as global variable across files
PCBDeque* PCBList;
PIDDeque* priorityList[4];  // 0 -> priority_zero, ... 3 -> inactive
pid_t pidCount = 0;         // global variable which assigns PID to new process,
                            // incremented by one each time

int ticks;
char* logFileName;
int logfd;
TerminalHistory* curr_history;

/**
 * @brief Counts the number of arguments in a command
 *
 * @param args NULL-terminated list of arguments
 * @return int Number of arguments in the list
 */
int num_args(char** args) {
  int i = 0;
  while (args[i] != NULL) {
    i++;
  }
  return i;
}

/**
 * @brief Creates a PennFAT filesystem in the file named fs_name
 *
 * @param fs_name Name of the filesystem file
 * @param blocks_in_fat Number of blocks in the FAT
 * @param block_size_config Specifies block size
 * @return int 1 if error, 0 if success
 */
int mkfs(const char* fs_name, int blocks_in_fat, int block_size_config) {
  // Error Handling
  if (blocks_in_fat < 1 || blocks_in_fat > 32) {
    fprintf(stderr, " mkfs: invalid number of blocks in FAT\n");
    return 1;
  }

  if (block_size_config < 0 || block_size_config > 4) {
    fprintf(stderr, " mkfs: invalid block size configuration\n");
    return 1;
  }

  // Case on block_size_config
  int block_size;
  switch (block_size_config) {
    case 0:
      block_size = 256;
      break;
    case 1:
      block_size = 512;
      break;
    case 2:
      block_size = 1024;
      break;
    case 3:
      block_size = 2048;
      break;
    case 4:
      block_size = 4096;
      break;
  }

  int fat_size = block_size * blocks_in_fat;
  int num_fat_entries = fat_size / 2;
  int data_region_size = block_size * (num_fat_entries - 1);

  if ((block_size = 4096) && (blocks_in_fat == 32)) {
    data_region_size = block_size * (num_fat_entries - 2);
  }

  // Create File
  int fd = open(fs_name, O_RDWR | O_CREAT | O_TRUNC, 0666);

  // Error Handling
  if (fd == -1) {
    fprintf(stderr, " mkfs: error creating file\n");
    return 1;
  }

  // Write FAT
  uint16_t fat[num_fat_entries];
  fat[0] = (blocks_in_fat << 8) | block_size_config;
  fat[1] = 0xFFFF;
  for (int i = 2; i < num_fat_entries; i++) {
    fat[i] = 0x0000;
  }

  // Write FAT to File
  if (write(fd, fat, fat_size) == -1) {
    fprintf(stderr, " mkfs: error writing FAT to file\n");
    return 1;
  }

  // Write Data Region

  // Use ftruncate to set the size of the file system
  if (ftruncate(fd, fat_size + data_region_size) == -1) {
    fprintf(stderr, " mkfs: error setting file system size\n");
    return 1;
  }

  // Close File
  if (close(fd) == -1) {
    fprintf(stderr, " mkfs: error closing file\n");
    return 1;
  }

  return 0;
}

/**
 * @brief Simulates shell for Standalone FAT
 *
 * @param argc An integer that indicates how many arguments were entered on the
 * command line when the program was started
 * @param argv An array of pointers to arrays of character objects
 * @return int returns EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc, char* argv[]) {
  // For Read & Parsing
  char command[MAX_LINE_LEN];
  struct parsed_command* parsed_command;

  // FAT Variables
  int num_blocks = 0;  // Number of Blocks in FAT
  int block_size = 0;  // Block Size

  // Registering Signal Handlers
  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);

  while (1) {
    // Prompt
    fprintf(stderr, "%s", PROMPT);

    // Read Command
    ssize_t num_bytes = read(STDIN_FILENO, command, MAX_LINE_LEN);
    if (num_bytes == -1) {
      fprintf(stderr, "Error while reading\n");
      return EXIT_FAILURE;
      // CTL + D
    } else if (num_bytes == 0) {
      return EXIT_SUCCESS;
    } else {
      command[num_bytes] = '\0';
    }

    // Parse Command
    int parse_result = parse_command(command, &parsed_command);

    // Handle Parse Error
    if (parse_result == -1) {
      fprintf(stderr, " parser system call error\n");
      return EXIT_FAILURE;
    }

    if (parse_result > 0) {
      fprintf(stderr, " parser specific error\n");
      return EXIT_FAILURE;
    }

    // Empty Command
    if (parsed_command->num_commands == 0) {
      free(parsed_command);
      continue;
    }

    // Execute Commands
    char** args = parsed_command->commands[0];
    if (strcmp(args[0], "mkfs") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd != -1) {
        fprintf(stderr, " mkfs: unexpected command\n");
        free(parsed_command);
        continue;
      }

      // Check # of Arguments
      if (num_args(args) != 4) {
        fprintf(stderr, " mkfs: invalid number of arguments\n");
        free(parsed_command);
        continue;
      }

      // Extract Arguments
      char* fs_name = args[1];
      int blocks_in_fat = atoi(args[2]);
      int block_size_config = atoi(args[3]);

      // Call mkfs
      if (mkfs(fs_name, blocks_in_fat, block_size_config) == 1) {
        free(parsed_command);
        continue;
      };

    } else if (strcmp(args[0], "mount") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd != -1) {
        fprintf(stderr, "mkfs: unexpected command\n");
        free(parsed_command);
        continue;
      }

      // Check # of Arguments
      if (num_args(args) != 2) {
        fprintf(stderr, "mkfs: invalid number of arguments\n");
        free(parsed_command);
        continue;
      }

      // Extract Arguments
      char* fs_name = args[1];

      // Call Mount
      int mount_res = mount(fs_name, &num_blocks, &block_size);
      if (mount_res == -1) {
        free(parsed_command);
        continue;
      };

      fs_fd = mount_res;

    } else if (strcmp(args[0], "unmount") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd == -1) {
        fprintf(stderr, "unmount: unexpected command\n");
        free(parsed_command);
        continue;
      }

      // Unmount
      if (munmap(fat, num_blocks * block_size) == -1) {
        fprintf(stderr, "unmount: error unmapping file\n");
        close(fs_fd);
        free(parsed_command);
        continue;
      }

      // Close File
      if (close(fs_fd) == -1) {
        fprintf(stderr, "unmount: error closing file\n");
        free(parsed_command);
        continue;
      }

      // Reset Variables
      fs_fd = -1;
      fat = NULL;
      num_blocks = 0;
      block_size = 0;

    } else if (strcmp(args[0], "touch") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd == -1) {
        fprintf(stderr, "touch: filesystem not mounted\n");
        free(parsed_command);
        continue;
      }

      // Check # of Arguments
      int arg_count = num_args(args);

      // Call k_touch for each file
      for (int i = 1; i < arg_count; i++) {
        if (k_touch(args[i]) == -1) {
          fprintf(stderr, "touch: error creating file\n");
          free(parsed_command);
          continue;
        };
      }

    } else if (strcmp(args[0], "mv") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd == -1) {
        fprintf(stderr, "mv: filesystem not mounted\n");
        free(parsed_command);
        continue;
      }

      // Check # of Arguments
      if (num_args(args) != 3) {
        fprintf(stderr, "mv: invalid number of arguments\n");
        free(parsed_command);
        continue;
      }

      // Extract Arguments
      char* source_file = args[1];
      char* dest_file = args[2];

      // Call k_mv
      if (k_mv(source_file, dest_file) == -1) {
        fprintf(stderr, " mv: error creating file\n");
        free(parsed_command);
        continue;
      };

    } else if (strcmp(args[0], "rm") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd == -1) {
        fprintf(stderr, "rm: filesystem not mounted\n");
        free(parsed_command);
        continue;
      }

      // Check # of Arguments
      int arg_count = num_args(args);

      // Call k_unlink for each file
      for (int i = 1; i < arg_count; i++) {
        if (k_unlink(args[i]) == -1) {
          fprintf(stderr, "rm: error removing file\n");
          free(parsed_command);
          continue;
        };
      }

    } else if (strcmp(args[0], "cat") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd == -1) {
        fprintf(stderr, "cat: filesystem not mounted\n");
        free(parsed_command);
        continue;
      }

      // Argument Casing
      int arg_count = num_args(args);

      if ((arg_count == 3) &&
          (strcmp(args[1], "-w") == 0 || strcmp(args[1], "-a") == 0)) {
        int fd_write;

        if (strcmp(args[1], "-w") == 0) {
          // Open File (Write)
          fd_write = k_open(args[2], F_WRITE);
        } else if (strcmp(args[1], "-a") == 0) {
          // Open File (Append)
          fd_write = k_open(args[2], F_APPEND);
        }

        if (fd_write == -1) {
          fprintf(stderr, "cat: error opening file\n");
          free(parsed_command);
          continue;
        }

        while (1) {
          char buffer[1024];
          ssize_t num_bytes = k_read(STDIN_FILENO, 1024, buffer);
          if (num_bytes == -1) {
            fprintf(stderr, "read error\n");
            free(parsed_command);
            continue;
          } else if (num_bytes == 0) {
            break;
          } else {
            buffer[num_bytes] = '\0';
          }

          if (k_write(fd_write, buffer, num_bytes) == -1) {
            fprintf(stderr, "cat: error writing to file\n");
            free(parsed_command);
            continue;
          }
        }

        // Close Files
        k_close(fd_write);
      } else if (arg_count >= 2 && strcmp(args[arg_count - 2], "-w") != 0 &&
                 strcmp(args[arg_count - 2], "-a") != 0) {
        for (int i = 1; i < arg_count; i++) {
          int fd_read;

          fd_read = k_open(args[i], F_READ);

          if (fd_read == -1) {
            fprintf(stderr, "cat: error opening file\n");
            free(parsed_command);
            continue;
          }
          while (1) {
            char buffer[1024];
            ssize_t num_bytes = k_read(fd_read, 1024, buffer);
            if (num_bytes == -1) {
              fprintf(stderr, " cat: error reading from file\n");
              free(parsed_command);
              continue;
            } else if (num_bytes == 0) {
              break;
            } else {
              buffer[num_bytes] = '\0';
            }

            if (k_write(STDOUT_FILENO, buffer, num_bytes) == -1) {
              fprintf(stderr, " cat: error writing to stdout\n");
              free(parsed_command);
              continue;
            }
          }

          // Close files
          k_close(fd_read);
        }
      } else if (arg_count >= 2 && strcmp(args[arg_count - 2], "-w") == 0) {
        for (int i = 1; i < arg_count - 2; i++) {
          int fd_read;
          int fd_write;

          fd_read = k_open(args[i], F_READ);
          fd_write = k_open(args[arg_count - 1], F_WRITE);

          if (fd_write == -1 || fd_read == -1) {
            fprintf(stderr, " cat: error opening input/output file\n");
            free(parsed_command);
            continue;
          }

          while (1) {
            char buffer[1024];
            ssize_t num_bytes = k_read(fd_read, 1024, buffer);
            if (num_bytes == -1) {
              fprintf(stderr, " cat: error reading from file\n");
              free(parsed_command);
              continue;
            } else if (num_bytes == 0) {
              break;
            } else {
              buffer[num_bytes] = '\0';
            }

            if (k_write(fd_write, buffer, num_bytes) == -1) {
              fprintf(stderr, " cat: error writing to stdout\n");
              free(parsed_command);
              continue;
            }
          }

          // Close Files
          k_close(fd_read);
          k_close(fd_write);
        }
      } else if (arg_count >= 2 && strcmp(args[arg_count - 2], "-a") == 0) {
        for (int i = 1; i < arg_count - 2; i++) {
          int fd_read;
          int fd_write;

          fd_read = k_open(args[i], F_READ);
          fd_write = k_open(args[arg_count - 1], F_APPEND);

          if (fd_read == -1 || fd_write == -1) {
            fprintf(stderr, "cat: error opening input/output file\n");
            free(parsed_command);
            continue;
          }

          while (1) {
            char buffer[1024];
            ssize_t num_bytes = k_read(fd_read, 1024, buffer);
            if (num_bytes == -1) {
              fprintf(stderr, "cat: error reading from file\n");
              free(parsed_command);
              continue;
            } else if (num_bytes == 0) {
              break;
            } else {
              buffer[num_bytes] = '\0';
            }

            if (k_write(fd_write, buffer, num_bytes) == -1) {
              fprintf(stderr, "cat: error writing to output file\n");
              free(parsed_command);
              continue;
            }
          }

          // Close Files
          k_close(fd_read);
          k_close(fd_write);
        }
      } else {
        fprintf(stderr, "cat: invalid arguments\n");
        free(parsed_command);
        continue;
      }
    } else if (strcmp(args[0], "cp") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd == -1) {
        fprintf(stderr, "cp: filesystem not mounted\n");
        free(parsed_command);
        continue;
      }

      int arg_count = num_args(args);

      // Check # of Arguments
      if (arg_count != 3 && arg_count != 4) {
        fprintf(stderr, "cp: invalid number of arguments\n");
        free(parsed_command);
        continue;
      }

      // cp SOURCE DEST
      if (arg_count == 3) {
        // Open SOURCE to read
        int fd_read = k_open(args[1], F_READ);
        if (fd_read == -1) {
          fprintf(stderr, "cp: error opening source file\n");
          free(parsed_command);
          continue;
        }

        // Open DEST to write
        int fd_write = k_open(args[2], F_WRITE);
        if (fd_write == -1) {
          fprintf(stderr, "cp: error opening destination file\n");
          free(parsed_command);
          continue;
        }

        // Read from SOURCE and write to DEST
        while (1) {
          char buffer[1024];
          int num_bytes = k_read(fd_read, 1024, buffer);
          if (num_bytes == -1) {
            fprintf(stderr, "cp: error reading from source file\n");
            free(parsed_command);
            continue;
          } else if (num_bytes == 0) {
            break;
          } else {
            buffer[num_bytes] = '\0';
          }

          if (k_write(fd_write, buffer, num_bytes) == -1) {
            fprintf(stderr, "cp: error writing to destination file\n");
            free(parsed_command);
            continue;
          }
        }

        // Close files
        k_close(fd_read);
        k_close(fd_write);
      }

      // cp -h SOURCE DEST
      if (arg_count == 4 && strcmp(args[1], "-h") == 0) {
        // Open SOURCE to read (from host OS)
        int fd_read = open(args[2], O_RDONLY);
        if (fd_read == -1) {
          fprintf(stderr, "cp: error opening source file from host OS\n");
          free(parsed_command);
          continue;
        }

        // Open DEST to write
        int fd_write = k_open(args[3], F_WRITE);
        if (fd_write == -1) {
          fprintf(stderr, "cp: error opening destination file\n");
          free(parsed_command);
          continue;
        }

        // Read from SOURCE and write to DEST
        while (1) {
          char buffer[1024];
          ssize_t num_bytes = read(fd_read, buffer, 1024);
          if (num_bytes == -1) {
            fprintf(stderr, "cp: error reading from source file\n");
            free(parsed_command);
            continue;
          } else if (num_bytes == 0) {
            break;
          } else {
            buffer[num_bytes] = '\0';
          }

          if (k_write(fd_write, buffer, num_bytes) == -1) {
            fprintf(stderr, "cp: error writing to destination file\n");
            free(parsed_command);
            continue;
          }
        }

        // Close files
        close(fd_read);
        k_close(fd_write);
      }

      // cp SOURCE -h DEST
      if (arg_count == 4 && strcmp(args[2], "-h") == 0) {
        // Open SOURCE to read
        int fd_read = k_open(args[1], F_READ);
        if (fd_read == -1) {
          fprintf(stderr, "cp: error opening source file\n");
          free(parsed_command);
          continue;
        }

        // Open DEST to write (to host OS)
        int fd_write = open(args[3], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_write == -1) {
          fprintf(stderr, "cp: error opening destination file from host OS\n");
          free(parsed_command);
          continue;
        }

        // Read from SOURCE and write to DEST
        while (1) {
          char buffer[1024];
          ssize_t num_bytes = k_read(fd_read, 1024, buffer);
          if (num_bytes == -1) {
            fprintf(stderr, "cp: error reading from source file\n");
            free(parsed_command);
            continue;
          } else if (num_bytes == 0) {
            break;
          } else {
            buffer[num_bytes] = '\0';
          }

          if (write(fd_write, buffer, num_bytes) == -1) {
            fprintf(stderr, "cp: error writing to destination file\n");
            free(parsed_command);
            continue;
          }
        }

        // Close files
        k_close(fd_read);
        close(fd_write);
      }

    } else if (strcmp(args[0], "chmod") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd == -1) {
        fprintf(stderr, "chmod: filesystem not mounted\n");
        free(parsed_command);
        continue;
      }

      // Check # of Arguments
      if (num_args(args) != 3) {
        fprintf(stderr, "chmod: invalid number of arguments\n");
        free(parsed_command);
        continue;
      }

      // Get permission type (-/+ rwx)
      char* perm_str = args[1];

      int permission_val = 0;
      int offset = 1;

      // Check if the first character is valid
      if (perm_str[0] != '-' && perm_str[0] != '+') {
        fprintf(
            stderr,
            "Invalid permission format: First character must be '+' or '-'\n");
        free(parsed_command);
        continue;
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
            fprintf(stderr,
                    "Invalid permission format: Unknown permission character "
                    "'%c'\n",
                    perm_str[offset]);
            free(parsed_command);
            continue;
        }
        offset++;
      }

      // Call k_chmod
      if (k_chmod(args[2], permission_val, perm_str[0]) == -1) {
        fprintf(stderr, "chmod: error changing permissions\n");
        free(parsed_command);
        continue;
      };

    } else if (strcmp(args[0], "ls") == 0) {
      // Check if Filesystem Mounted
      if (fs_fd == -1) {
        fprintf(stderr, "ls: filesystem not mounted\n");
        free(parsed_command);
        continue;
      }

      // Check # of Arguments
      if (num_args(args) != 1) {
        fprintf(stderr, "ls: invalid number of arguments\n");
        free(parsed_command);
        continue;
      }

      // Call k_ls
      if (k_ls(NULL, STDOUT_FILENO) == -1) {
        fprintf(stderr, "ls: error listing files\n");
        free(parsed_command);
        continue;
      };

    } else {
      fprintf(stderr, "command not found: %s\n", args[0]);
    }
  }

  return EXIT_SUCCESS;
}