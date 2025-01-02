#include "os_errors.h"

const char* str_error(int errnum) {
  switch (errnum) {
    case EARG:
      return "Invalid argument(s)";
    case ESIG:
      return "Invalid signal";
    case ENOENT:
      return "No such file or directory";
    case ECHILD:
      return "No child processes";
    case EDEQUE:
      return "Deque error";
    case EFD:
      return "File descriptor/table error";
    case EIO:
      return "I/O error";
    case EPARSE:
      return "Invalid parse";
    case EPERM:
      return "Invalid permissions";
    case ECMD:
      return "Invalid command";
    case EHOST:
      return "Host OS error";
    case EJOB:
      return "Invalid job / job doesn't exist";
    default:
      return "Unknown error";
  }
}

void u_error(const char* error_message) {
  const char* err_string = str_error(P_ERRNO);
  if (error_message && *error_message) {
    // If we have an error message to print, we print it first
    // Format the string and variable into the buffer
    char formatted_msg[strlen(error_message) + strlen(err_string) + 6];
    snprintf(formatted_msg, sizeof(formatted_msg), "[%s]: %s\n", error_message,
             err_string);
    k_write(2, formatted_msg, strlen(formatted_msg));
  } else {
    char formatted_msg[strlen(err_string) + 2];
    snprintf(formatted_msg, sizeof(formatted_msg), "%s\n", err_string);
    k_write(2, formatted_msg, strlen(formatted_msg));
  }
}