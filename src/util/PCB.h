
#ifndef PCB_H_
#define PCB_H_

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "./PIDDeque.h"
#include "./globals.h"
#include "./spthread.h"

// Represents a job
typedef struct pcb_st {
  pid_t pid;
  int status;  // referenced in macros.h
  pid_t parent_pid;
  spthread_t curr_thread;
  PIDDeque* child_pids;
  PIDDeque* status_changes;
  int blocking;  // 0: not blocking, 1: blocking
  int priority;
  int sleep_duration;  // if not sleeping, set sleep_duration = -1;
  char* process_name;
  int stop_time;
  bool is_background;
  int process_fdt[1024];
  struct parsed_command* parsed;
  int job_id;
} pcb;
#endif  // JOB_H_
