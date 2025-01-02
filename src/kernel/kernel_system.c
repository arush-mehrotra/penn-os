#include "./kernel_system.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

pid_t s_spawn(void* (*func)(void*),
              char* argv[],
              int input_file,
              int output_file,
              char* process_name,
              bool is_background,
              struct parsed_command* parsed) {
  spthread_t childThread;
  spthread_create(&childThread, NULL, *func, argv);
  pcb* parent = k_get_proc();
  pcb* child = k_proc_create(parent, childThread, input_file, output_file,
                             process_name, is_background, parsed);

  // Write log
  char message[100];
  sprintf(message, "[%3d]\tCREATE   \t%d\t%d\t%-15s\n", ticks, child->pid,
          child->priority, process_name);
  s_log(message);

  return child->pid;
}

pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang) {
  int res = k_waitpid(pid, wstatus, nohang);
  if (res == -1) {
    P_ERRNO = ECHILD;
  }
  return res;
}

int s_kill(pid_t pid, int signal) {
  int res = k_send_signal(pid, signal);
  if (res == -1) {
    P_ERRNO = ESIG;
  }
  return res;
}

void s_exit(void) {
  k_exit();
}

int s_handle_fg(pid_t pid) {
  return k_handle_fg(pid);
}

int s_handle_bg(pid_t pid) {
  return k_handle_bg(pid);
}

int s_nice(pid_t pid, int priority) {
  if (priority < 0 || priority > 2) {
    P_ERRNO = EARG;
    return -1;
  }
  return k_change_priority(pid, priority);
}

void s_sleep(unsigned int ticks) {
  k_sleep(ticks);
}

void s_log(char* message) {
  k_write_log(message);
}

void s_ps() {
  k_ps();
}

/********************************/
/*     FAT S Functions          */
/********************************/

int s_touch(char* fname) {
  return k_touch(fname);
}

int s_mv(char* source_file, char* dest_file) {
  return k_mv(source_file, dest_file);
}

int s_chmod(char* file_name, int perm, char modify) {
  return k_chmod(file_name, perm, modify);
}

int s_open(const char* fname, int mode) {
  int fd = k_open(fname, mode);
  if (fd == -1) {
    P_ERRNO = EFD;
    return -1;
  }

  // Add to process-level FDT
  pcb* curr_job = k_get_proc();
  curr_job->process_fdt[fd] = fd;

  return fd;
}

int s_read(int fd, int n, char* buf) {
  return k_read(fd, n, buf);
}

int s_write(int fd, const char* str, int n) {
  return k_write(fd, str, n);
}

int s_close(int fd) {
  // Close on process-level FDT
  pcb* curr_job = k_get_proc();
  curr_job->process_fdt[fd] = -1;

  return k_close(fd);
}

int s_unlink(const char* fname) {
  return k_unlink(fname);
}

int s_lseek(int fd, int offset, int whence) {
  return k_lseek(fd, offset, whence);
}

int s_ls(const char* filename, int output_fd) {
  return k_ls(filename, output_fd);
}

int s_findperm(char* filename) {
  return k_findperm(filename);
}