#include "./fat_helper.h"
#include "./fat_globals.h"

int mount(char* fs_name, int* num_blocks, int* block_size) {
  fs_fd = open(fs_name, O_RDWR);

  // Error Handling
  if (fs_fd == -1) {
    P_ERRNO = EHOST;
    u_error("mount: error opening file");
    return -1;
  }

  // Retrieve Size of Fat

  // Move the file pointer to the beginning of the file
  if (lseek(fs_fd, 0, SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("mount: error seeking to beginning of file");
    close(fs_fd);
    return -1;
  }

  uint16_t buffer;
  if (read(fs_fd, &buffer, 2) != 2) {
    P_ERRNO = EHOST;
    u_error("mount: error reading from file");
    close(fs_fd);
    return -1;
  }

  (*num_blocks) = buffer >> 8;

  switch (buffer & 0xFF) {
    case 0:
      (*block_size) = 256;
      break;
    case 1:
      (*block_size) = 512;
      break;
    case 2:
      (*block_size) = 1024;
      break;
    case 3:
      (*block_size) = 2048;
      break;
    case 4:
      (*block_size) = 4096;
      break;
  }

  // Mount
  fat = mmap(NULL, (*num_blocks) * (*block_size), PROT_READ | PROT_WRITE,
             MAP_SHARED, fs_fd, 0);

  // Initialize Global File Descriptor Table
  char* stdin = "stdin";
  char* stderr = "stderr";
  char* stdout = "stdout";

  strncpy(g_fdt[0].name, stdin, 32);
  g_fdt[0].perm = F_READ;
  g_fdt[0].offset = 0;
  g_fdt[0].firstBlock = 0;
  g_fdt[0].size = 0;
  g_fdt[0].open = true;

  strncpy(g_fdt[1].name, stdout, 32);
  g_fdt[1].perm = F_WRITE;
  g_fdt[1].offset = 0;
  g_fdt[1].firstBlock = 0;
  g_fdt[1].size = 0;
  g_fdt[1].open = true;

  strncpy(g_fdt[2].name, stderr, 32);
  g_fdt[2].perm = F_WRITE;
  g_fdt[2].offset = 0;
  g_fdt[2].firstBlock = 0;
  g_fdt[2].size = 0;
  g_fdt[2].open = true;

  for (int i = 3; i < 1024; i++) {
    g_fdt[i].perm = F_NONE;
    g_fdt[i].offset = 0;
    g_fdt[i].firstBlock = 0;
    g_fdt[i].size = 0;
    g_fdt[i].open = false;
  }

  g_counter = 3;

  return fs_fd;
}

int k_findperm(char* file_name) {
  dir_entry directory;
  loc_entry location;

  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  int file_exists = k_file_exists(file_name, &directory, &location);

  if (file_exists == 0) {
    P_ERRNO = ENOENT;
    u_error("findperm");
    return -1;
  } else if (file_exists == 2) {
    return -1;
  }

  return directory.perm;
}

/************************************************/
/*               User k_Functions               */
/************************************************/

int k_touch(char* file_name) {
  // Check if file exists
  dir_entry directory;
  loc_entry location;
  int file_exists = k_file_exists(file_name, &directory, &location);

  int num_blocks;
  int block_size;
  k_metadata(&num_blocks, &block_size);

  // File does not exist
  if (file_exists == 0) {
    // Seek to directory entry for new file
    if (lseek(fs_fd, num_blocks * block_size, SEEK_SET) == -1) {
      P_ERRNO = EHOST;
      u_error("touch: error seeking to root directory");
      return -1;
    }

    uint16_t curr_directory = 1;
    uint16_t prev_directory = 0;

    dir_entry new_directory;

    bool addedEntry = false;

    while ((curr_directory != 0xFFFF) && !addedEntry) {
      // Seek to the current block
      if (lseek(fs_fd,
                num_blocks * block_size + ((curr_directory - 1) * block_size),
                SEEK_SET) == -1) {
        P_ERRNO = EHOST;
        u_error("touch: error seeking to root directory");
        return -1;
      }

      int num_bytes_read = 0;
      // Create 64 byte buffer
      unsigned char buffer[64];
      for (int i = 0; i < (block_size) / 64; i++) {
        int bytes_read = read(fs_fd, buffer, 64);
        if (bytes_read == -1) {
          P_ERRNO = EHOST;
          u_error("touch: error reading root directory");
          return 2;
        }

        memcpy(&new_directory, buffer, sizeof(dir_entry));

        if (new_directory.name[0] == DEL_FILE ||
            new_directory.name[0] == END_DIR) {
          // Update the directory entry
          strncpy(new_directory.name, file_name, 32);
          new_directory.size = 0;
          new_directory.firstBlock = 0;
          new_directory.type = TYPE_FILE;
          new_directory.perm = PERM_READ_WRITE;
          new_directory.time = time(NULL);

          // Seek back to the start of the directory block and write the updated
          // buffer
          if (lseek(fs_fd, -bytes_read, SEEK_CUR) == -1 ||
              write(fs_fd, &new_directory, sizeof(dir_entry)) == -1) {
            P_ERRNO = EHOST;
            u_error("touch: error writing updated directory entry");
            return -1;
          }

          addedEntry = true;
          break;
        }

        num_bytes_read += bytes_read;
      }
      prev_directory = curr_directory;
      curr_directory = (fat)[curr_directory];
    }

    // Need to create new block for root directory entry

    if (!addedEntry) {
      int rootEntry = k_open_entry();
      // print our root, prev dir
      if (rootEntry == -1) {
        P_ERRNO = EFD;
        u_error("touch: no open entries in FAT");
        return -1;
      }
      (fat)[rootEntry] = 0xFFFF;
      (fat)[prev_directory] = rootEntry;
      msync(fat, num_blocks * block_size, MS_SYNC);

      // Seek to new block
      if (lseek(fs_fd, num_blocks * block_size + ((rootEntry - 1) * block_size),
                SEEK_SET) == -1) {
        P_ERRNO = EHOST;
        u_error("touch: error seeking to new block");
        return -1;
      }

      // Zero out entire block & seek back
      char tmp[block_size];
      memset(tmp, 0, block_size);
      write(fs_fd, tmp, block_size);

      if (lseek(fs_fd, num_blocks * block_size + ((rootEntry - 1) * block_size),
                SEEK_SET) == -1) {
        P_ERRNO = EHOST;
        u_error("touch: error seeking to new block");
        return -1;
      }

      // Update the directory entry
      strncpy(new_directory.name, file_name, 32);
      new_directory.size = 0;
      new_directory.firstBlock = 0;
      new_directory.type = TYPE_FILE;
      new_directory.perm = PERM_READ_WRITE;
      new_directory.time = time(NULL);

      // Write the updated directory entry
      if (write(fs_fd, &new_directory, sizeof(dir_entry)) == -1) {
        P_ERRNO = EHOST;
        u_error("touch: error writing updated directory entry");
        return -1;
      }
    }
  }

  // File exists
  if (file_exists == 1) {
    // Seek to directory entry for existing file (use location)
    if (lseek(fs_fd,
              num_blocks * block_size + ((location.block - 1) * block_size) +
                  location.offset,
              SEEK_SET) == -1) {
      P_ERRNO = EHOST;
      u_error("touch: error seeking to directory entry");
      return -1;
    }

    // Update the time
    time_t curr_time = time(NULL);
    directory.time = curr_time;

    // Write the updated directory entry
    if (write(fs_fd, &directory, sizeof(dir_entry)) == -1) {
      P_ERRNO = EHOST;
      u_error("touch: error writing updated directory entry");
      return -1;
    }
  }

  if (file_exists == 2) {
    return -1;
  }

  return 0;
}

int k_mv(char* source_file, char* dest_file) {
  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  dir_entry directory_1;
  loc_entry location_1;

  dir_entry directory_2;
  loc_entry location_2;

  int returnVal_1 = k_file_exists(source_file, &directory_1, &location_1);
  int returnVal_2 = k_file_exists(dest_file, &directory_2, &location_2);

  if (returnVal_1 == 0) {
    P_ERRNO = ENOENT;
    u_error("mv: source file");
    return -1;
  }
  if (returnVal_1 == 2) {
    return -1;
  }

  // Check if src_file has read permissions
  int src_perm = k_findperm(source_file);

  if (src_perm == 0 || src_perm == 2) {
    P_ERRNO = EPERM;
    u_error("mv: source file does not have read permissions");
    return -1;
  }

  if (returnVal_2 == 2) {
    return -1;
  } else if (returnVal_2 == 1) {
    // Check if dest_file has write permissions
    int dest_perm = k_findperm(dest_file);

    // Print dest_perm
    if (dest_perm == 0 || dest_perm == 4 || dest_perm == 5) {
      P_ERRNO = EPERM;
      u_error("mv: destination file does not have write permissions");
      return -1;
    }

    if (k_unlink(dest_file) < 0) {
      return -1;
    }
  }

  // Seek to directory entry for existing file (use location_1)
  if (lseek(fs_fd,
            num_blocks * block_size + ((location_1.block - 1) * block_size) +
                location_1.offset,
            SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("mv: error seeking to directory entry");
    return -1;
  }

  // Update the name
  strncpy(directory_1.name, dest_file, 32);
  directory_1.time = time(NULL);

  // Write the updated directory entry
  if (write(fs_fd, &directory_1, sizeof(dir_entry)) == -1) {
    P_ERRNO = EHOST;
    u_error("mv: error writing updated directory entry");
    return -1;
  }

  return 0;
}

int k_chmod(char* file_name, int perm, char modify) {
  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  dir_entry directory;
  loc_entry location;

  int file_exists = k_file_exists(file_name, &directory, &location);

  if (file_exists == 0) {
    P_ERRNO = ENOENT;
    u_error("chmod: error: file does not exist");
    return -1;
  } else if (file_exists == 2) {
    return -1;
  }

  // Seek to directory entry for existing file (use location)
  if (lseek(fs_fd,
            num_blocks * block_size + ((location.block - 1) * block_size) +
                location.offset,
            SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("chmod: error seeking to directory entry");
    return -1;
  }

  // Update perm
  if (modify == '+') {
    switch (perm) {
      case 1:
        if (directory.perm == PERM_READ) {
          directory.perm = PERM_READ_EXEC;
        } else if (directory.perm == PERM_READ_WRITE ||
                   directory.perm == PERM_READ_WRITE_EXEC) {
          directory.perm = PERM_READ_WRITE_EXEC;
        }
        break;
      case 2:
        if (directory.perm == PERM_NONE) {
          directory.perm = PERM_WRITE;
        } else if (directory.perm == PERM_READ) {
          directory.perm = PERM_READ_WRITE;
        } else if (directory.perm == PERM_READ_EXEC) {
          directory.perm = PERM_READ_WRITE_EXEC;
        }
        break;
      case 3:
        if (directory.perm == PERM_READ) {
          directory.perm = PERM_READ_WRITE_EXEC;
        }
      case 4:
        if (directory.perm == PERM_NONE) {
          directory.perm = PERM_READ;
        } else if (directory.perm == PERM_WRITE) {
          directory.perm = PERM_READ_WRITE;
        }
        break;
      case 5:
        if (directory.perm == PERM_NONE) {
          directory.perm = PERM_READ_EXEC;
        } else if (directory.perm == PERM_WRITE ||
                   directory.perm == PERM_READ_WRITE) {
          directory.perm = PERM_READ_WRITE_EXEC;
        } else if (directory.perm == PERM_READ) {
          directory.perm = PERM_READ_EXEC;
        }
        break;
      case 6:
        if (directory.perm == PERM_NONE || directory.perm == PERM_READ ||
            directory.perm == PERM_WRITE) {
          directory.perm = PERM_READ_WRITE;
        } else if (directory.perm == PERM_READ_WRITE) {
          directory.perm = PERM_READ_WRITE_EXEC;
        }
        break;
      case 7:
        directory.perm = PERM_READ_WRITE_EXEC;
        break;
      default:
        break;
    }
  } else if (modify == '-') {
    switch (perm) {
      // Removing Executable Permissions
      case 1:
        if (directory.perm == PERM_READ_EXEC) {
          directory.perm = PERM_READ;
        } else if (directory.perm == PERM_READ_WRITE_EXEC) {
          directory.perm = PERM_READ_WRITE;
        }
        break;
      // Removing Write Permissions
      case 2:
        if (directory.perm == PERM_WRITE) {
          directory.perm = PERM_NONE;
        } else if (directory.perm == PERM_READ_WRITE) {
          directory.perm = PERM_READ;
        } else if (directory.perm == PERM_READ_WRITE_EXEC) {
          directory.perm = PERM_READ_EXEC;
        }
        break;
      // Removing Exec-Write Permissions
      case 3:
        if (directory.perm == PERM_WRITE) {
          directory.perm = PERM_NONE;
        } else if (directory.perm == PERM_READ_EXEC) {
          directory.perm = PERM_READ;
        } else if (directory.perm == PERM_READ_WRITE) {
          directory.perm = PERM_READ;
        } else if (directory.perm == PERM_READ_WRITE_EXEC) {
          directory.perm = PERM_READ;
        }
      // Removing Read Permissions
      case 4:
        if (directory.perm == PERM_READ) {
          directory.perm = PERM_NONE;
        } else if (directory.perm == PERM_READ_WRITE) {
          directory.perm = PERM_WRITE;
        } else if (directory.perm == PERM_READ_WRITE_EXEC) {
          // throw an error
          P_ERRNO = EPERM;
          u_error(
              "chmod: invalid modify argument have read-write-exec "
              "permissions but trying to remove read");
          return -1;
        }
        break;
      // Removing Read-Exec Permissions
      case 5:
        if (directory.perm == PERM_READ || directory.perm == PERM_READ_EXEC) {
          directory.perm = PERM_NONE;
        } else if (directory.perm == PERM_READ_WRITE_EXEC) {
          directory.perm = PERM_WRITE;
        }
        break;
      // Removing Read-Write Permissions
      case 6:
        if (directory.perm == PERM_READ_EXEC ||
            directory.perm == PERM_READ_WRITE_EXEC) {
          // throw an error
          P_ERRNO = EPERM;
          u_error(
              "chmod: invalid modify argument have read-exec or "
              "read-write-exec permissions but trying to remove read-write");
          return -1;
        } else {
          directory.perm = PERM_NONE;
        }
        break;
      case 7:
        directory.perm = PERM_NONE;
        break;
      default:
        break;
    }
  } else {
    P_ERRNO = EARG;
    u_error("chmod: invalid modify argument");
    return -1;
  }

  // Write the updated directory entry
  if (write(fs_fd, &directory, sizeof(dir_entry)) == -1) {
    P_ERRNO = EHOST;
    u_error("chmod: error writing updated directory entry");
    return -1;
  }

  return 0;
}

int k_ls_all(int output_fd) {
  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  // Seek to root directory
  if (lseek(fs_fd, num_blocks * block_size, SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("k_ls: error seeking to root directory");
    return -1;
  }

  int curr_block = 1;
  char buffer2[4096];
  memset(buffer2, 0, sizeof(buffer2));
  while (curr_block != 0xFFFF) {
    int num_bytes_read = 0;
    // Create 64 byte buffer
    unsigned char buffer[64];
    for (int i = 0; i < (block_size) / 64; i++) {
      int bytes_read = read(fs_fd, buffer, 64);
      if (bytes_read == -1) {
        P_ERRNO = EHOST;
        u_error("k_ls: error reading root directory");
        return -1;
      }

      dir_entry directory;
      memcpy(&directory, buffer, sizeof(dir_entry));

      if (directory.name[0] == DEL_FILE || directory.name[0] == END_DIR) {
        continue;
      }

      char perm_str[4] = "---";

      // Set permission string based on the provided perm values
      switch (directory.perm) {
        case 2:
          perm_str[1] = 'w';
          break;
        case 4:
          perm_str[0] = 'r';
          break;
        case 5:
          perm_str[0] = 'r';
          perm_str[2] = 'x';
          break;
        case 6:
          perm_str[0] = 'r';
          perm_str[1] = 'w';
          break;
        case 7:
          perm_str[0] = 'r';
          perm_str[1] = 'w';
          perm_str[2] = 'x';
          break;
        default:
          break;
      }

      // Format time using ctime
      char* time_tostr = ctime(&directory.time);
      // Remove the newline character from ctime output
      time_tostr[strlen(time_tostr) - 1] = '\0';

      // Print the directory
      if (output_fd == 1) {
        char message[100];
        sprintf(message, "%3u %s %5u %s %s\n", directory.firstBlock, perm_str,
                directory.size, time_tostr, directory.name);
        k_write(output_fd, message, strlen(message));
      } else {
        // Write to output file
        sprintf(buffer2 + strlen(buffer2), "%3u %s %5u %s %s\n",
                directory.firstBlock, perm_str, directory.size, time_tostr,
                directory.name);
      }

      num_bytes_read += bytes_read;
    }
    curr_block = (fat)[curr_block];
    if (curr_block != 0xFFFF) {
      if (lseek(fs_fd,
                num_blocks * block_size + ((curr_block - 1) * block_size),
                SEEK_SET) == -1) {
        P_ERRNO = EHOST;
        u_error("k_ls: error seeking to next block");
        return -1;
      }
    }
  }

  if (output_fd != 1) {
    k_write(output_fd, buffer2, strlen(buffer2));
  }

  return 0;
}

/************************************************/
/*               Helper Functions               */
/************************************************/

int k_file_exists(char* file_name, dir_entry* directory, loc_entry* location) {
  bool file_found = false;

  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  // Seek to root directory
  if (lseek(fs_fd, num_blocks * block_size, SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("k_file_exists: error seeking to root directory");
    return 2;
  }

  // Check each entry of root directory
  uint16_t curr_directory = 1;

  while (curr_directory != 0xFFFF && !file_found) {
    int num_bytes_read = 0;
    // Create 64 byte buffer
    unsigned char buffer[64];
    for (int i = 0; i < (block_size) / 64; i++) {
      int bytes_read = read(fs_fd, buffer, 64);
      if (bytes_read == -1) {
        P_ERRNO = EHOST;
        u_error("k_file_exists: error reading root directory");
        return 2;
      }

      memcpy(directory, buffer, sizeof(dir_entry));

      if (directory->name[0] == END_DIR) {
        break;
      }

      // Check if first 32 bytes matches file_name
      if (strncmp(directory->name, file_name, 32) == 0 &&
          directory->name[0] != DEL_FILE && directory->name[0] != END_DIR) {
        file_found = true;
        location->block = curr_directory;
        location->offset = num_bytes_read;
        break;
      }

      num_bytes_read += bytes_read;
    }
    curr_directory = (fat)[curr_directory];
    if (curr_directory != 0xFFFF) {
      if (lseek(fs_fd,
                num_blocks * block_size + ((curr_directory - 1) * block_size),
                SEEK_SET) == -1) {
        P_ERRNO = EHOST;
        u_error("k_file_exists: error seeking to next block");
        return 2;
      }
    }
  }

  if (file_found) {
    return 1;
  } else {
    return 0;
  }
}

void k_metadata(int* num_blocks, int* block_size) {
  *num_blocks = (fat)[0] >> 8;

  switch ((fat)[0] & 0xFF) {
    case 0:
      *block_size = 256;
      break;
    case 1:
      *block_size = 512;
      break;
    case 2:
      *block_size = 1024;
      break;
    case 3:
      *block_size = 2048;
      break;
    case 4:
      *block_size = 4096;
      break;
  }
}

int k_open_entry() {
  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  int i = 2;
  while ((fat)[i] != 0x0000 && i < (num_blocks * block_size / 2)) {
    i++;
  }

  if (i == (num_blocks * block_size / 2)) {
    P_ERRNO = EFD;
    u_error("k_open_entry: no open entries in FAT");
    return -1;
  }

  return i;
}

int update_file_size_dir(char* file_name, uint32_t new_size) {
  // check if file exists
  dir_entry directory;
  loc_entry location;

  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  int file_exists = k_file_exists(file_name, &directory, &location);

  if (file_exists == 0) {
    P_ERRNO = ENOENT;
    u_error("update_file_size_dir: error: file does not exist");
    return -1;
  } else if (file_exists == 2) {
    return -1;
  }

  // Seek to directory entry for existing file (use location)
  if (lseek(fs_fd,
            num_blocks * block_size + ((location.block - 1) * block_size) +
                location.offset,
            SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("update_file_size_dir: error seeking to directory entry");
    return -1;
  }

  // Update the size
  directory.size = new_size;

  // Write the updated directory entry
  if (write(fs_fd, &directory, sizeof(dir_entry)) == -1) {
    P_ERRNO = EHOST;
    u_error("update_file_size_dir: error writing updated directory entry");
    return -1;
  }

  return 0;
}

int update_size_fdt(char* file_name, uint32_t new_size) {
  for (int i = 0; i < 1024; i++) {
    if (g_fdt[i].open && strcmp(g_fdt[i].name, file_name) == 0) {
      g_fdt[i].size = new_size;
      return 0;
    }
  }

  return -1;
}

int update_offset_fdt(char* file_name, uint32_t new_offset) {
  for (int i = 0; i < 1024; i++) {
    if (g_fdt[i].open && strcmp(g_fdt[i].name, file_name) == 0) {
      g_fdt[i].offset = new_offset;
      return 0;
    }
  }

  return -1;
}

/**
 * @brief Allocates new blocks in the FAT for a file if needed
 *
 * @param file_name File to update size for
 * @param new_size Size to update to
 * @return int 0 if successful, 1 if error
 */
int update_file_size(char* file_name, uint32_t new_size) {
  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  // Check if file exists
  dir_entry directory;
  loc_entry location;

  int file_exists = k_file_exists(file_name, &directory, &location);

  if (file_exists == 0) {
    P_ERRNO = ENOENT;
    u_error("update_file_size: error: file does not exist");
    return -1;
  } else if (file_exists == 2) {
    return -1;
  }

  // Check original size
  uint32_t original_size = directory.size;

  int bytes_remaining = new_size - original_size;

  // Check how much is remaining in file's last block
  int leftover_space = original_size % block_size;

  // Don't need to allocate new blocks in the FAT
  if (bytes_remaining <= leftover_space) {
    return 0;
  }

  // Need to allocate new_blocks in the FAT
  int num_blocks_to_allocate =
      (bytes_remaining - leftover_space + block_size - 1) / block_size;

  int last_block = directory.firstBlock;

  // Find last block
  while ((fat)[last_block] != 0xFFFF) {
    last_block = (fat)[last_block];
  }

  // Allocate new blocks
  for (int i = 0; i < num_blocks_to_allocate; i++) {
    int new_block = k_open_entry();
    if (new_block == -1) {
      P_ERRNO = EFD;
      u_error("update_file_size: no open entries in FAT");
      return -1;
    }
    (fat)[last_block] = new_block;
    (fat)[new_block] = 0xFFFF;
    last_block = new_block;
  }

  return 0;
}

/************************************************/
/*               k_Functions                    */
/************************************************/

int k_unlink(const char* fName) {
  char* file_name = (char*)fName;

  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  dir_entry directory;
  loc_entry location;

  int file_exists = k_file_exists(file_name, &directory, &location);

  if (file_exists == 0) {
    P_ERRNO = ENOENT;
    u_error("k_unlink: error: file does not exist");
    return -1;
  } else if (file_exists == 2) {
    return -1;
  }

  // Seek to directory entry for existing file (use location)
  if (lseek(fs_fd,
            num_blocks * block_size + ((location.block - 1) * block_size) +
                location.offset,
            SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("k_unlink: error seeking to directory entry");
    return -1;
  }

  for (int i = 0; i < 1024; i++) {
    if (g_fdt[i].open && strcmp(g_fdt[i].name, file_name) == 0) {
      P_ERRNO = EFD;
      u_error("k_unlink: file is currently open, can not delete");
      return -1;
    }
  }

  directory.name[0] = DEL_FILE;

  // Check if need to make directory.name[0] = END_DIR

  // Check if last block of directory
  if (location.offset == block_size - 64) {
    directory.name[0] = END_DIR;
  }

  // Check if next block in directory is END_DIR
  if (lseek(fs_fd,
            num_blocks * block_size + ((location.block - 1) * block_size) +
                location.offset + 64,
            SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("k_unlink: error seeking to next directory entry");
    return -1;
  }

  unsigned char buffer[64];
  int bytes_read = read(fs_fd, buffer, 64);
  if (bytes_read == -1) {
    P_ERRNO = EHOST;
    u_error("k_unlink: error reading root directory");
    return -1;
  }

  dir_entry next_directory;
  memcpy(&next_directory, buffer, sizeof(dir_entry));

  if (next_directory.name[0] == END_DIR) {
    directory.name[0] = END_DIR;
  }

  // Write the updated directory entry
  // Seek to directory entry for existing file (use location)
  if (lseek(fs_fd,
            num_blocks * block_size + ((location.block - 1) * block_size) +
                location.offset,
            SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("k_unlink: error seeking to directory entry");
    return -1;
  }

  if (write(fs_fd, &directory, sizeof(dir_entry)) == -1) {
    P_ERRNO = EHOST;
    u_error("k_unlink: error writing updated directory entry");
    return -1;
  }

  // Update FAT
  int curr_block = directory.firstBlock;
  if (curr_block == 0) {
    return 0;
  }
  while ((fat)[curr_block] != 0xFFFF) {
    int next_block = (fat)[curr_block];
    (fat)[curr_block] = 0x0000;
    msync(fat, num_blocks * block_size, MS_SYNC);
    curr_block = next_block;
  }
  (fat)[curr_block] = 0x0000;
  msync(fat, num_blocks * block_size, MS_SYNC);

  return 0;
}

int k_ls(const char* filename, int output_fd) {
  if (filename == NULL) {
    if (k_ls_all(output_fd) != 0) {
      return -1;
    };
    return 0;
  }

  char* file_name = (char*)filename;

  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  // Check if file exists
  dir_entry directory;
  loc_entry location;
  int file_exists = k_file_exists(file_name, &directory, &location);

  if (file_exists == 0) {
    P_ERRNO = ENOENT;
    u_error("ls: error: file does not exist");
    return -1;
  } else if (file_exists == 2) {
    return -1;
  }

  // Seek to directory entry for existing file (use location)
  if (lseek(fs_fd,
            num_blocks * block_size + ((location.block - 1) * block_size) +
                location.offset,
            SEEK_SET) == -1) {
    P_ERRNO = EHOST;
    u_error("ls: error seeking to directory entry");
    return -1;
  }

  char perm_str[4] = "---";

  // Set permission string based on the provided perm values
  switch (directory.perm) {
    case 2:
      perm_str[1] = 'w';
      break;
    case 4:
      perm_str[0] = 'r';
      break;
    case 5:
      perm_str[0] = 'r';
      perm_str[2] = 'x';
      break;
    case 6:
      perm_str[0] = 'r';
      perm_str[1] = 'w';
      break;
    case 7:
      perm_str[0] = 'r';
      perm_str[1] = 'w';
      perm_str[2] = 'x';
      break;
    default:
      break;
  }

  // Format time using ctime
  char* time_tostr = ctime(&directory.time);
  // Remove the newline character from ctime output
  time_tostr[strlen(time_tostr) - 1] = '\0';

  // Print the directory
  if (output_fd == 1) {
    char message[100];
    sprintf(message, "%3u %s %5u %s %s\n", directory.firstBlock, perm_str,
            directory.size, time_tostr, directory.name);
    k_write(output_fd, message, strlen(message));
  } else {
    // Write to output file
    k_open(g_fdt[output_fd].name, F_WRITE);
    char buffer[64];
    sprintf(buffer, "%3u %s %5u %s %s\n", directory.firstBlock, perm_str,
            directory.size, time_tostr, directory.name);
    k_write(output_fd, buffer, strlen(buffer));
    k_close(output_fd);
  }

  return 0;
}

int k_close(int fd) {
  if ((fd > 1023) | (fd < 3)) {
    P_ERRNO = EFD;
    u_error("k_close: file descriptor out of range");
    return -1;
  }

  // Check the Global File Descriptor Table
  if (g_fdt[fd].open == false) {
    P_ERRNO = EFD;
    u_error("k_close: file descriptor is not open");
    return -1;
  }

  g_fdt[fd].open = false;
  return 0;
}

int k_lseek(int fd, int offset, int whence) {
  if ((fd > 1023) | (fd < 3)) {
    P_ERRNO = EFD;
    u_error("k_lseek: file descriptor out of range");
    return -1;
  }

  // Check the Global File Descriptor Table
  if (g_fdt[fd].open == false) {
    P_ERRNO = EFD;
    u_error("k_lseek: file descriptor is not open");
    return -1;
  }

  global_fdt* fd_entry = &g_fdt[fd];

  // Determine the new file position
  int new_pos;
  switch (whence) {
    case F_SEEK_SET:
      new_pos = offset;
      break;
    case F_SEEK_CUR:
      new_pos = fd_entry->offset + offset;
      break;
    case F_SEEK_END:
      new_pos = fd_entry->size + offset;
      break;
    default:
      P_ERRNO = EARG;
      u_error("k_lseek: invalid whence argument");
      return -1;
  }

  // Check if new position is within bounds
  if (new_pos < 0) {
    P_ERRNO = EARG;
    u_error(" k_lseek: new position out of bounds");
    return -1;
  }

  if (new_pos > fd_entry->size) {
    if (update_file_size_dir(fd_entry->name, new_pos) == -1) {
      return -1;
    };
    if (update_file_size(fd_entry->name, new_pos) == -1) {
      return -1;
    };
  }

  // Update offset
  if (update_offset_fdt(fd_entry->name, new_pos) == -1) {
    return -1;
  };

  return new_pos;
}

int k_write(int fd, const char* str, int n) {
  // Check if the file descriptor is valid and the file is open for writing
  if (fd < 0 || fd > 1023) {
    perror("k_write: out of bound");
    return -1;
  } else if (!g_fdt[fd].open || g_fdt[fd].perm == F_READ) {
    perror("k_write: file is not open for writing");
    return -1;
  }

  // Write to STDERR/STDOUT
  if (fd == 1) {
    if (write(STDOUT_FILENO, str, n) == -1) {
      perror("k_write: error writing to STDOUT");
      return -1;
    }
    return n;
  } else if (fd == 2) {
    if (write(STDERR_FILENO, str, n) == -1) {
      perror("k_write: error writing to STDERR");
      return -1;
    }
    return n;
  } else if (fd == 0) {
    perror("k_write: cannot write to STDIN");
    return -1;
  }

  // Get the file descriptor entry
  global_fdt* fd_entry = &g_fdt[fd];

  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  // Check if the file exists
  dir_entry directory;
  loc_entry location;

  int file_exists = k_file_exists(fd_entry->name, &directory, &location);

  if (file_exists == 0) {
    perror("k_write: error: file does not exist");
    return -1;
  } else if (file_exists == 2) {
    perror("k_write: error while checking existence of file");
    return -1;
  }

  // File exists, check directory entry to see if need to allocate spot in FAT
  int entry;
  if (directory.firstBlock == 0) {
    entry = k_open_entry();
    if (entry == -1) {
      perror("k_write: no open entries in FAT");
      return -1;
    }
    (fat)[entry] = 0xFFFF;
    msync(fat, num_blocks * block_size, MS_SYNC);

    directory.firstBlock = entry;

    // Write the updated directory entry (lseek then write)
    if (lseek(fs_fd,
              num_blocks * block_size + ((location.block - 1) * block_size) +
                  location.offset,
              SEEK_SET) == -1) {
      perror("k_write: error seeking to directory entry");
      return -1;
    }

    if (write(fs_fd, &directory, sizeof(dir_entry)) == -1) {
      perror("k_write: error writing updated directory entry");
      return -1;
    }
  }

  // Check with what permissions the file is open
  if (fd_entry->perm == F_WRITE) {
    // Find block where current offset is to
    int curr_block = directory.firstBlock;

    int total_offset = fd_entry->offset;

    int offset_blocks = fd_entry->offset / block_size;
    int rounded_blocks = (fd_entry->offset % block_size == 0)
                             ? offset_blocks
                             : offset_blocks + 1;
    int final_blocks = rounded_blocks - 1;

    for (int i = 0; i < final_blocks; i++) {
      curr_block = (fat)[curr_block];
      total_offset -= block_size;
    }

    int remaining_bytes_block = block_size - total_offset;

    // Check if the remaining bytes in the block are enough to write the string
    if (n <= remaining_bytes_block) {
      // Seek to the block
      if (lseek(fs_fd,
                num_blocks * block_size + ((curr_block - 1) * block_size +
                                           fd_entry->offset % block_size),
                SEEK_SET) == -1) {
        perror("k_write: error seeking to block");
        return -1;
      }
      // Write the string to the block
      if (write(fs_fd, str, n) == -1) {
        perror("k_write: error writing to block");
        return -1;
      }

      // Update the size (if needed) & offset of the file
      if (fd_entry->offset + n > directory.size) {
        if (update_file_size_dir(fd_entry->name, directory.size + n) == -1) {
          perror("k_write: error updating file size");
          return -1;
        }

        fd_entry->size += n;
      }

      fd_entry->offset += n;

      curr_block = directory.firstBlock;

      int offset_blocks =
          fd_entry->offset / block_size;  // This performs integer division.
      int rounded_blocks =
          (fd_entry->offset % block_size == 0)
              ? offset_blocks
              : offset_blocks + 1;  // Rounds up if there's a remainder.
      int final_blocks = rounded_blocks - 1;  // Subtract 1 after rounding up.

      // Find which block offset corresponds to
      for (int i = 0; i < final_blocks; i++) {
        curr_block = (fat)[curr_block];
      }

      // Update the FAT
      int prev_block = fat[curr_block];
      fat[curr_block] = 0xFFFF;
      msync(fat, num_blocks * block_size, MS_SYNC);

      while (prev_block != 0xFFFF) {
        curr_block = prev_block;
        prev_block = fat[curr_block];
        fat[curr_block] = 0x0000;
        msync(fat, num_blocks * block_size, MS_SYNC);
      }

      return n;
    }

    int bytes_remaining_to_write = n;

    // Write the remaining bytes in the block
    if (lseek(fs_fd,
              num_blocks * block_size + ((curr_block - 1) * block_size +
                                         fd_entry->offset % block_size),
              SEEK_SET) == -1) {
      perror("k_write: error seeking to block");
      return -1;
    }

    if (write(fs_fd, str, remaining_bytes_block) == -1) {
      perror("k_write: error writing to block");
      return -1;
    }

    bytes_remaining_to_write -= remaining_bytes_block;

    while (bytes_remaining_to_write > 0) {
      if (fat[curr_block] != 0xFFFF) {
        curr_block = fat[curr_block];

        if (lseek(fs_fd,
                  num_blocks * block_size + ((curr_block - 1) * block_size),
                  SEEK_SET) == -1) {
          perror("k_write: error seeking to block");
          return -1;
        }

        int num_bytes_to_write = bytes_remaining_to_write;

        if (bytes_remaining_to_write > block_size) {
          num_bytes_to_write = block_size;
        }

        if (write(fs_fd, str + n - bytes_remaining_to_write,
                  num_bytes_to_write) == -1) {
          perror("k_write: error writing to block");
          return -1;
        }

        bytes_remaining_to_write -= num_bytes_to_write;

      } else {
        int new_block = k_open_entry();
        if (new_block == -1) {
          perror("k_write: no open entries in FAT");
          return n - bytes_remaining_to_write;
        }

        (fat)[curr_block] = new_block;
        (fat)[new_block] = 0xFFFF;
        msync(fat, num_blocks * block_size, MS_SYNC);
        curr_block = new_block;

        if (lseek(fs_fd,
                  num_blocks * block_size + ((curr_block - 1) * block_size),
                  SEEK_SET) == -1) {
          perror("k_write: error seeking to new block");
          return -1;
        }

        int num_bytes_to_write = bytes_remaining_to_write;

        if (bytes_remaining_to_write > block_size) {
          num_bytes_to_write = block_size;
        }

        if (write(fs_fd, str + n - bytes_remaining_to_write,
                  num_bytes_to_write) == -1) {
          perror("k_write: error writing to new block");
          return -1;
        }

        bytes_remaining_to_write -= num_bytes_to_write;
      }
    }

    // Update the size (if needed) & offset of the file
    if (fd_entry->offset + n > directory.size) {
      if (update_file_size_dir(fd_entry->name, directory.size + n) == -1) {
        perror("k_write: error updating file size");
        return -1;
      }

      fd_entry->size += n;
    }

    fd_entry->offset += n;

    curr_block = directory.firstBlock;
    int offset_blocks_2 = fd_entry->offset / block_size;
    int rounded_blocks_2 = (fd_entry->offset % block_size == 0)
                               ? offset_blocks_2
                               : offset_blocks_2 + 1;
    int final_blocks_2 = rounded_blocks_2 - 1;

    // Find which block offset corresponds to
    for (int i = 0; i < final_blocks_2; i++) {
      curr_block = (fat)[curr_block];
    }

    // Update the FAT
    int prev_block = fat[curr_block];
    fat[curr_block] = 0xFFFF;
    msync(fat, num_blocks * block_size, MS_SYNC);

    while (prev_block != 0xFFFF) {
      curr_block = prev_block;
      prev_block = fat[curr_block];
      fat[curr_block] = 0x0000;
      msync(fat, num_blocks * block_size, MS_SYNC);
    }

    return n - bytes_remaining_to_write;

  } else if (fd_entry->perm == F_APPEND) {
    // Seek to end of file
    if (k_lseek(fd, 0, F_SEEK_END) == -1) {
      perror("k_write: error seeking to end of file");
      return -1;
    }

    // Find last block of the file
    int curr_block = directory.firstBlock;
    while ((fat)[curr_block] != 0xFFFF) {
      curr_block = (fat)[curr_block];
    }

    int remaining_bytes_block = block_size - (directory.size % block_size);

    // Check if the remaining bytes in the last block are enough to write the
    // string
    if (n <= remaining_bytes_block) {
      // Seek to the last block
      if (lseek(fs_fd,
                num_blocks * block_size + ((curr_block - 1) * block_size +
                                           directory.size % block_size),
                SEEK_SET) == -1) {
        perror("k_write: error seeking to last block");
        return -1;
      }

      // Write the string to the last block
      if (write(fs_fd, str, n) == -1) {
        perror("k_write: error writing to last block");
        return -1;
      }

      // Update the size & offset of the file
      if (update_file_size_dir(fd_entry->name, directory.size + n) == -1) {
        perror("k_write: error updating file size");
        return -1;
      }

      fd_entry->size += n;
      fd_entry->offset += n;

      return n;
    }

    int remaining_bytes_to_write = n;

    // Write the remaining bytes in the last block
    if (lseek(fs_fd,
              num_blocks * block_size +
                  ((curr_block - 1) * block_size + directory.size % block_size),
              SEEK_SET) == -1) {
      perror("k_write: error seeking to last block");
      return -1;
    }

    if (write(fs_fd, str, remaining_bytes_block) == -1) {
      perror("k_write: error writing to last block");
      return -1;
    }

    remaining_bytes_to_write -= remaining_bytes_block;

    // Allocate new blocks in the FAT
    int num_blocks_to_allocate =
        (remaining_bytes_to_write + block_size - 1) / block_size;

    for (int i = 0; i < num_blocks_to_allocate; i++) {
      int new_block = k_open_entry();
      if (new_block == -1) {
        perror("k_write: no open entries in FAT");
        return n - remaining_bytes_to_write;
      }

      (fat)[curr_block] = new_block;
      (fat)[new_block] = 0xFFFF;
      curr_block = new_block;

      // Write the remaining bytes to the new block
      if (lseek(fs_fd,
                num_blocks * block_size + ((curr_block - 1) * block_size),
                SEEK_SET) == -1) {
        perror("k_write: error seeking to new block");
        return -1;
      }

      int num_bytes_to_write = remaining_bytes_to_write;

      if (remaining_bytes_to_write > block_size) {
        num_bytes_to_write = block_size;
      }

      if (write(fs_fd, str + n - remaining_bytes_to_write,
                num_bytes_to_write) == -1) {
        perror("k_write: error writing to new block");
        return -1;
      }

      remaining_bytes_to_write -= num_bytes_to_write;
    }

    // Update the size & offset of the file
    if (update_file_size_dir(fd_entry->name,
                             directory.size + n - remaining_bytes_to_write) ==
        1) {
      perror("k_write: error updating file size");
      return -1;
    }

    fd_entry->size += n - remaining_bytes_to_write;
    fd_entry->offset += n - remaining_bytes_to_write;

    return n - remaining_bytes_to_write;
  } else {
    perror("k_write: file is not open for writing or appending");
    return -1;
  }
}

int k_open(const char* fName, int mode) {
  char* file_name = (char*)fName;

  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  // Check if file exists
  dir_entry directory;
  loc_entry location;

  int file_exists = k_file_exists(file_name, &directory, &location);

  // Error in k_file_exists
  if (file_exists == 2) {
    return -1;
  }

  switch (mode) {
    case F_WRITE:
      // File does not exist
      if (file_exists == 0) {
        if (k_touch(file_name) == -1) {
          return -1;
        }

        // File created
        // Add file to Global File Descriptor Table
        g_fdt[g_counter].open = true;
        g_fdt[g_counter].perm = F_WRITE;
        g_fdt[g_counter].size = 0;
        g_fdt[g_counter].offset = 0;
        strcpy(g_fdt[g_counter].name, file_name);
        g_counter++;

        return (g_counter - 1);
      }

      // File already exists with F_WRITE or F_APPEND
      for (int i = 0; i < 1024; i++) {
        if (g_fdt[i].open && strcmp(g_fdt[i].name, file_name) == 0 &&
            (g_fdt[i].perm == F_WRITE || g_fdt[i].perm == F_APPEND)) {
          P_ERRNO = EFD;
          u_error(
              "open: file is already open with write or append permissions");
          return -1;
        }
      }

      // Check if file has write permissions
      if (directory.perm != 2 && directory.perm != 6 && directory.perm != 7) {
        P_ERRNO = EPERM;
        u_error("open: file does not have write permissions");
        return -1;
      }

      // Add file to Global File Descriptor Table
      g_fdt[g_counter].open = true;
      g_fdt[g_counter].perm = F_WRITE;
      g_fdt[g_counter].size = 0;
      g_fdt[g_counter].offset = 0;
      strcpy(g_fdt[g_counter].name, file_name);
      g_counter++;

      // Update size in directory (because truncated)
      if (update_file_size_dir(file_name, 0) == -1) {
        return -1;
      }

      return (g_counter - 1);

    case F_READ:
      if (file_exists == 0) {
        P_ERRNO = ENOENT;
        u_error("open: error: file does not exist");
        return -1;
      }

      // Check if file has read permissions
      if (directory.perm != 4 && directory.perm != 5 && directory.perm != 6 &&
          directory.perm != 7) {
        P_ERRNO = EPERM;
        u_error("open: file does not have read permissions");
        return -1;
      }

      // File already exists
      // Add file to Global File Descriptor Table
      g_fdt[g_counter].open = true;
      g_fdt[g_counter].perm = F_READ;
      g_fdt[g_counter].size = directory.size;
      g_fdt[g_counter].offset = 0;
      strcpy(g_fdt[g_counter].name, file_name);
      g_counter++;

      return (g_counter - 1);
    case F_APPEND:
      if (file_exists == 0) {
        if (k_touch(file_name) == -1) {
          return -1;
        }
        // File created
        // Add file to Global File Descriptor Table
        g_fdt[g_counter].open = true;
        g_fdt[g_counter].perm = F_APPEND;
        g_fdt[g_counter].size = 0;
        g_fdt[g_counter].offset = 0;
        strcpy(g_fdt[g_counter].name, file_name);
        g_counter++;

        return (g_counter - 1);
      }

      // Check if file has read and write permissions
      if (directory.perm != 6 && directory.perm != 7) {
        P_ERRNO = EPERM;
        u_error("open: file does not have read and write permissions");
        return -1;
      }

      // File already exists with F_WRITE or F_APPEND
      for (int i = 0; i < 1024; i++) {
        if (g_fdt[i].open && strcmp(g_fdt[i].name, file_name) == 0 &&
            (g_fdt[i].perm == F_WRITE || g_fdt[i].perm == F_APPEND)) {
          P_ERRNO = EFD;
          u_error(
              "open: file is already open with write or append permissions");
          return -1;
        }
      }

      // File already exists (don't truncate)

      // Add file to Global File Descriptor Table
      g_fdt[g_counter].open = true;
      g_fdt[g_counter].perm = F_APPEND;
      g_fdt[g_counter].size = directory.size;
      g_fdt[g_counter].offset = directory.size;
      strcpy(g_fdt[g_counter].name, file_name);
      g_counter++;

      return (g_counter - 1);
    default:
      P_ERRNO = EARG;
      u_error("open: invalid mode");
      return -1;
  }
}

int k_read(int fd, int n, char* buf) {
  if (fd < 0 || fd > 1023) {
    P_ERRNO = EFD;
    u_error("k_read: file descriptor out of range");
    return -1;
  }
  if (!g_fdt[fd].open) {
    P_ERRNO = EFD;
    u_error("k_read: file is not open for reading");
    return -1;
  }
  if (g_fdt[fd].perm == F_NONE) {
    P_ERRNO = EFD;
    u_error("k_read: file is not open for reading");
    return -1;
  }

  // Read from STDIN
  if (fd == 0) {
    int bytes_read = read(STDIN_FILENO, buf, n);
    if (bytes_read == -1) {
      P_ERRNO = EHOST;
      u_error("k_read: error reading from STDIN");
      return -1;
    }
    return bytes_read;
  } else if (fd == 1 || fd == 2) {
    P_ERRNO = EFD;
    u_error(
        "k_read: file descriptor is not open for reading (reading "
        "from STDERR/STDOUT)");
    return -1;
  }

  // Get the file descriptor entry
  global_fdt* fd_entry = &g_fdt[fd];

  int num_blocks;
  int block_size;

  k_metadata(&num_blocks, &block_size);

  // Check if the file exists
  dir_entry directory;
  loc_entry location;

  int file_exists = k_file_exists(fd_entry->name, &directory, &location);

  if (file_exists == 0) {
    P_ERRNO = ENOENT;
    u_error("k_read: error: file does not exist");
    return -1;
  } else if (file_exists == 2) {
    return -1;
  }

  if (fd_entry->size == fd_entry->offset) {
    return 0;
  }

  // Read the file and store in buf
  int bytes_read = 0;
  int curr_block = directory.firstBlock;
  int curr_offset = fd_entry->offset;

  while (curr_offset >= block_size) {
    curr_block = (fat)[curr_block];
    curr_offset -= block_size;
  }

  while (bytes_read < n && fd_entry->offset < fd_entry->size) {
    // Seek to the block
    if (lseek(fs_fd,
              num_blocks * block_size +
                  ((curr_block - 1) * block_size + curr_offset),
              SEEK_SET) == -1) {
      P_ERRNO = EHOST;
      u_error("k_read: error seeking to block");
      return -1;
    }

    // Read the block
    int num_bytes_to_read = n - bytes_read;
    if (num_bytes_to_read > block_size - curr_offset) {
      num_bytes_to_read = block_size - curr_offset;
    }

    if (fd_entry->size - fd_entry->offset < num_bytes_to_read) {
      num_bytes_to_read = fd_entry->size - fd_entry->offset;
    }

    if (read(fs_fd, buf + bytes_read, num_bytes_to_read) == -1) {
      P_ERRNO = EHOST;
      u_error("k_read: error reading block");
      return -1;
    }

    bytes_read += num_bytes_to_read;
    fd_entry->offset += num_bytes_to_read;

    if (fd_entry->offset == fd_entry->size) {
      break;
    }

    curr_block = (fat)[curr_block];
    curr_offset = 0;
  }

  return bytes_read;
}