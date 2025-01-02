#ifndef KERNEL_SYSTEM_H
#define KERNEL_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "../fat/fat_helper.h"
#include "../util/globals.h"
#include "../util/os_errors.h"
#include "./kernel.h"

/**
 * @brief Create a child process that executes the function `func`.
 * The child will retain some attributes of the parent.
 *
 * @param func Function to be executed by the child process.
 * @param argv Null-terminated array of args, including the command name as
 * argv[0].
 * @param fd0 Input file descriptor.
 * @param fd1 Output file descriptor.
 * @param process_name Name of the process.
 * @return pid_t The process ID of the created child process.
 */
pid_t s_spawn(void* (*func)(void*),
              char* argv[],
              int fd0,
              int fd1,
              char* process_name,
              bool is_background,
              struct parsed_command* parsed);

/**
 * @brief Wait on a child of the calling process, until it changes state.
 * If `nohang` is true, this will not block the calling process and return
 * immediately.
 *
 * @param pid Process ID of the child to wait for.
 * @param wstatus Pointer to an integer variable where the status will be
 * stored.
 * @param nohang If true, return immediately if no child has exited.
 * @return pid_t The process ID of the child which has changed state on success,
 * -1 on error.
 */
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang);

/**
 * @brief Send a signal to a particular process.
 *
 * @param pid Process ID of the target proces.
 * @param signal Signal number to be sent.
 * @return 0 on success, -1 on error.
 */
int s_kill(pid_t pid, int signal);

/**
 * @brief Unconditionally exit the calling process.
 */
void s_exit(void);

/**
 * @brief Set the priority of the specified thread.
 *
 * @param pid Process ID of the target thread.
 * @param priority The new priorty value of the thread (0, 1, or 2)
 * @return 0 on success, -1 on failure.
 */
int s_nice(pid_t pid, int priority);

/**
 * @brief Suspends execution of the calling proces for a specified number of
 * clock ticks.
 *
 * This function is analogous to `sleep(3)` in Linux, with the behavior that the
 * system clock continues to tick even if the call is interrupted. The sleep can
 * be interrupted by a P_SIGTERM signal, after which the function will return
 * prematurely.
 *
 * @param ticks Duration of the sleep in system clock ticks. Must be greater
 * than 0.
 */
void s_sleep(unsigned int ticks);

/**
 * @brief Write a message to the system log.
 *
 * @param message Message to be written to the log.
 */
void s_log(char* message);

/**
 * @brief Prints information about all processes on the system.
 *
 */
void s_ps();

/**
 * @brief Creates the files if they do not exist, or updates their timestamp to
 * the current system time
 *
 * @param file_name File to create/update
 * @return int 0 if successful, -1 if error
 */
int s_touch(char* fname);

/**
 * @brief Renames the source file to the destination file
 *
 * @param source_file File to rename
 * @param dest_file New name of the file
 * @return int 0 if successful, -1 if error
 */
int s_mv(char* source_file, char* dest_file);

/**
 * @brief Changes permissions for a file
 *
 * @param file_name File to change permissions for
 * @param perm Permissions to change to
 * @param modify Either '+' or '-', corresponding to adding or removing
 * permissions
 * @return int 0 if successful, -1 if error
 */
int s_chmod(char* file_name, int perm, char modify);

/**
 * @brief Opens a file with the given name and mode
 *
 * @param fName File to open
 * @param mode F_READ, F_WRITE, or F_APPEND
 * @return int File descriptor if successful, -1 if error
 */
int s_open(const char* fname, int mode);

/**
 * @brief Read n bytes from the file referenced by fd
 *
 * @param fd File descriptor to read from
 * @param n Number of bytes to read
 * @param buf Buffer to store the read bytes
 * @return int Number of bytes read, 0 if EOF reached, -1 if error
 */
int s_read(int fd, int n, char* buf);

/**
 * @brief Write n bytes of the string referenced by str to the file fd and
 * increment the file pointer by n
 *
 * @param fd File descriptor to write to
 * @param str String to write
 * @param n Number of bytes to write
 * @return int Number of bytes written, -1 if error
 */
int s_write(int fd, const char* str, int n);

/**
 * @brief Close the file indicated by fd
 *
 * @param fd File descriptor to close
 * @return int 0 if successful, -1 if error
 */
int s_close(int fd);

/**
 * @brief Removes a file from the file system
 *
 * @param fName File to remove
 * @return int 0 if successful, -1 if error
 */
int s_unlink(const char* fname);

/**
 * @brief Repositions the file pointer for file indicated by fd to the offset
 * relative to whence
 *
 * @param fd File descriptor to reposition
 * @param offset Number of bytes to move the file pointer
 * @param whence F_SEEK_SET, F_SEEK_CUR, or F_SEEK_END
 * @return int 0 if successful, -1 if error
 */
int s_lseek(int fd, int offset, int whence);

/**
 * @brief List the filename/filenames in the current directory
 *
 * @param filename File to list (NULL if all files)
 * @param output_fd File to write the output to (STDOUT if NULL)
 * @return int 0 if successful, -1 if error
 */
int s_ls(const char* filename, int output_fd);

/**
 * @brief Returns the permission fo the filename
 *
 * @param filename File to get permission
 * @return int representing the permission of the file
 */
int s_findperm(char* filename);

/**
 * @brief Handles the bg command on specfied pid
 *
 * @param pid job to resume in the background, -1 if no job provided to bg
 * command
 * @return 0 on successs, -1 on error
 */
int s_handle_bg(pid_t pid);

/**
 * @brief Handles thefbg command on specfied pid
 *
 * @param pid job to resume in the foreground, -1 if no job provided to fg
 * command
 * @return 0 on successs, -1 on error
 */
int s_handle_fg(pid_t pid);
#endif