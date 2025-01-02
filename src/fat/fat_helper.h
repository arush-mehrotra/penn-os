#ifndef FAT_HELPER_H
#define FAT_HELPER_H

#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "../util/os_errors.h"
#include "./fat_globals.h"

// MACROS
#define TYPE_UNKNOWN 0
#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_LINK 4
#define PERM_NONE 0
#define PERM_WRITE 2
#define PERM_READ 4
#define PERM_READ_EXEC 5
#define PERM_READ_WRITE 6
#define PERM_READ_WRITE_EXEC 7

#define F_SEEK_SET 0
#define F_SEEK_CUR 1
#define F_SEEK_END 2

#define F_NONE 0
#define F_READ 1
#define F_WRITE 2
#define F_APPEND 3

#define END_DIR 0
#define DEL_FILE 1
#define CUR_FILE 2

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

typedef struct dir_entry {
  char name[32];
  uint32_t size;
  uint16_t firstBlock;
  uint8_t type;
  uint8_t perm;
  time_t time;
  char _BUFFER_[16];
} dir_entry;

typedef struct loc_entry {
  int block;
  int offset;
} loc_entry;

/**
 * @brief Mounts the file system
 *
 * @param fs_name Name of the file system to mount
 * @param num_blocks Number of blocks in the file system
 * @param block_size Size of each block in the file system
 * @return int 0 if successful, -1 if error
 */
int mount(char* fs_name, int* num_blocks, int* block_size);

/**
 * @brief Finds permissions for a file
 *
 * @param file_name Name of the file to find permissions for
 * @return int Permission of the file if successful, -1 if error
 */
int k_findperm(char* file_name);

/**
 * @brief Creates the files if they do not exist, or updates their timestamp to
 * the current system time
 *
 * @param file_name File to create/update
 * @return int 0 if successful, -1 if error
 */
int k_touch(char* file_name);

/**
 * @brief Renames the source file to the destination file
 *
 * @param source_file File to rename
 * @param dest_file New name of the file
 * @return int 0 if successful, -1 if error
 */
int k_mv(char* source_file, char* dest_file);

/**
 * @brief Changes permissions for a file
 *
 * @param file_name File to change permissions for
 * @param perm Permissions to change to
 * @param modify Either '+' or '-', corresponding to adding or removing
 * permissions
 * @return int 0 if successful, -1 if error
 */
int k_chmod(char* file_name, int perm, char modify);

/**
 * @brief List all files in the directory
 *
 * @param output_fd File to write the output to (STDOUT if NULL)
 *
 * @return int 0 if successful, -1 if error
 */
int k_ls_all(int output_fd);

/**
 * @brief Check if a file exists in the directory
 *
 * @param file_name File to check
 * @param directory [Output Parameter] Pointer to the directory entry of the
 * file
 * @param location [Output Parameter] Pointer to the location entry of the file
 * @return int 0 if file does not exist, 1 if file exists, 2 if error
 */
int k_file_exists(char* file_name, dir_entry* directory, loc_entry* location);

/**
 * @brief Retrieve the metadata of the file system
 *
 * @param num_blocks [Output Parameter] Pointer to the number of blocks in the
 * file system
 * @param block_size [Output Parameter] Pointer to the size of each block in the
 * file system
 */
void k_metadata(int* num_blocks, int* block_size);

/**
 * @brief Find the first open entry in the FAT
 *
 * @return int -1 if no open entries, otherwise the index of the first open
 * entry
 */
int k_open_entry();

/**
 * @brief Updates size of file in directory entry
 *
 * @param file_name File to update size for
 * @param new_size Size to update to
 * @return int 0 if successful, -1 if error
 */
int update_file_size(char* file_name, uint32_t new_size);

/**
 * @brief Updates the size of a file in the Global File Descriptor Table
 *
 * @param file_name File to update size for
 * @param new_size Size to update to
 * @return int 0 if successful, -1 if error
 */
int update_size_fdt(char* file_name, uint32_t new_size);

/**
 * @brief Updates the offset of a file in the Global File Descriptor Table
 *
 * @param file_name File to update offset for
 * @param new_offset Offset to update to
 * @return int 0 if successful, -1 if error
 */
int update_offset_fdt(char* file_name, uint32_t new_offset);

/**
 * @brief Allocates new blocks in the FAT for a file if needed
 *
 * @param file_name File to update size for
 * @param new_size Size to update to
 * @return int 0 if successful, -1 if error
 */
int update_file_size(char* file_name, uint32_t new_size);

/**
 * @brief Removes a file from the file system
 *
 * @param fName File to remove
 * @return int 0 if successful, -1 if error
 */
int k_unlink(const char* fname);

/**
 * @brief List the filename/filenames in the current directory
 *
 * @param filename File to list (NULL if all files)
 * @param output_fd File to write the output to (STDOUT if NULL)
 * @return int 0 if successful, 1 if error
 */
int k_ls(const char* filename, int output_fd);

/**
 * @brief Close the file indicated by fd
 *
 * @param fd File descriptor to close
 * @return int 0 if successful, -1 if error
 */
int k_close(int fd);

/**
 * @brief Repositions the file pointer for file indicated by fd to the offset
 * relative to whence
 *
 * @param fd File descriptor to reposition
 * @param offset Number of bytes to move the file pointer
 * @param whence F_SEEK_SET, F_SEEK_CUR, or F_SEEK_END
 * @return int 0 if successful, -1 if error
 */
int k_lseek(int fd, int offset, int whence);

/**
 * @brief Write n bytes of the string referenced by str to the file fd and
 * increment the file pointer by n
 *
 * @param fd File descriptor to write to
 * @param str String to write
 * @param n Number of bytes to write
 * @return int Number of bytes written, -1 if error
 */
int k_write(int fd, const char* str, int n);

/**
 * @brief Read n bytes from the file referenced by fd
 *
 * @param fd File descriptor to read from
 * @param n Number of bytes to read
 * @param buf Buffer to store the read bytes
 * @return int Number of bytes read, 0 if EOF reached, -1 if error
 */
int k_read(int fd, int n, char* buf);

/**
 * @brief Opens a file with the given name and mode
 *
 * @param fName File to open
 * @param mode F_READ, F_WRITE, or F_APPEND
 * @return int File descriptor if successful, -1 if error
 */
int k_open(const char* fName, int mode);

#endif