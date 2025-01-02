#ifndef OS_ERRORS_H_
#define OS_ERRORS_H_
#include "../fat/fat_helper.h"

extern int P_ERRNO;

// ERROR CODES
#define EARG 1    // Invalid argument(s)
#define ENOENT 2  // No such file or directory
#define ESIG 3    // Operation not permitted
#define ECHILD 4  // No child processes
#define EDEQUE 5  // Deque error
#define EFD 6     // File descriptor/table error
#define EIO 7     // I/O error
#define EPARSE 8  // Invalid parse
#define EPERM 9   // Invalid permissions
#define ECMD 10   // Invalid command
#define EHOST 11  // Host OS error
#define EJOB 12   // Invalid job / job doesn't exist

/**
 * @brief User function to write an error message
 *
 * @param error_message: the message to write out
 */
void u_error(const char* error_message);

#endif  // OS_ERRORS_H_