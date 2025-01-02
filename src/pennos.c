#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "util/spthread.h"

#include "kernel/kernel.h"
#include "kernel/kernel_system.h"
#include "kernel/shell.h"
#include "util/PCBDeque.h"
#include "util/PIDDeque.h"
#include "util/globals.h"
#include "util/macros.h"

#include "fat/fat_globals.h"
#include "fat/fat_helper.h"

#include "util/os_errors.h"

#define OPEN_FLAG 0644

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

// Declared as global variable across files
PCBDeque* PCBList;
PIDDeque* priorityList[4];  // 0 -> priority_zero, ... 3 -> inactive

pid_t pidCount = 0;  // global variable which assigns PID to new process,
                     // incremented by one each time
pid_t currentJob = 0;

pid_t fgJob = -1;
int ticks;
char* logFileName;
int logfd;
// Global Variables
int fs_fd = -1;          // File Descriptor for FAT
uint16_t* fat = NULL;    // FAT
global_fdt g_fdt[1024];  // Global File Descriptor Table
int g_counter = 0;
bool logged_out = false;
int num_bg_jobs = 0;
pid_t plus_pid = -1;
int P_ERRNO = 0;
TerminalHistory* curr_history;

/******************************************/
/*               SCHEDULER                */
/******************************************/

const int QUANTUM = 100;

static void signal_handler(int signum) {
  if (signum == SIGINT) {
    if (s_write(STDERR_FILENO, "\n", 1) == -1) {
      s_exit();
    }
    if (fgJob == 1 && (s_write(STDERR_FILENO, PROMPT, PROMPT_SIZE) == -1)) {
      s_exit();
    }
    if (fgJob > 1) {
      s_kill(fgJob, P_SIGTERM);
    }
  } else if (signum == SIGTSTP) {
    if (s_write(STDERR_FILENO, "\n", 1) == -1) {
      s_exit();
    }
    if (fgJob == 1 && (s_write(STDERR_FILENO, PROMPT, PROMPT_SIZE) == -1)) {
      s_exit();
    }
    if (fgJob > 1) {
      s_kill(fgJob, P_SIGSTOP);
    }
  }
}

static void updateplus_pid() {
  pcb* curr_job = NULL;
  curr_job = PCBDequeStopSearch(PCBList);
  if (curr_job == NULL) {
    curr_job = PCBDequeBackgroundSearch(PCBList);
  }
  if (curr_job == NULL || curr_job->status == STATUS_FINISHED ||
      curr_job->status == STATUS_TERMINATED) {
  } else {
    plus_pid = curr_job->pid;
  }
}
// signal handler for sigalarm
// can be left empty since we just need
// to know that the handler has gone off and not
// terminate when we get the signal.
static void alarm_handler(int signum) {}

static int select_job() {
  // The 1.5 ratios between priority levels are expected values
  // That means we just need to sample from a 9:6:4 distribution
  // In expectation, that will achieve the desired ratio

  // See which of the priority levels have an unfinished job
  int size0 = PIDDeque_Size(priorityList[0]);
  int size1 = PIDDeque_Size(priorityList[1]);
  int size2 = PIDDeque_Size(priorityList[2]);

  // Nothing in any queue
  if (size0 + size1 + size2 == 0) {
    // Something wrong -- there should always be a thread to run
    return -1;
  }
  // Only a job in priority 0
  else if ((size0) > 0 && (size1 + size2 == 0)) {
    return 0;
  }
  // Only a job in priority 1
  else if ((size1) > 0 && (size0 + size2 == 0)) {
    return 1;
  }
  // Only a job in priority 2
  else if ((size2) > 0 && (size0 + size1 == 0)) {
    return 2;
  }
  // Jobs in priority 0 and 1
  else if ((size0 > 0) && (size1 > 0) && (size2 == 0)) {
    int random = rand() % 5;
    if (random <= 2) {
      return 0;
    } else {
      return 1;
    }
  }
  // Jobs in priority 1 and 2
  else if ((size1 > 0) && (size2 > 0) && (size0 == 0)) {
    int random = rand() % 5;
    if (random <= 2) {
      return 1;
    } else {
      return 2;
    }
  }
  // Jobs in priority 0 and 2
  else if ((size0 > 0) && (size2 > 0) && (size1 == 0)) {
    int random = rand() % 13;
    if (random <= 8) {
      return 0;
    } else {
      return 2;
    }
  }
  // Jobs in all priorities
  else {
    int random = rand() % 19;
    if (random <= 8) {
      return 0;
    } else if (random <= 14) {
      return 1;
    } else {
      return 2;
    }
  }
}

static void add_job_back(pcb* this_pcb) {
  if (P_WIFRUNNING(this_pcb->status)) {
    int priority = this_pcb->priority;
    PIDDeque_Push_Back(priorityList[priority], this_pcb->pid);
  } else if (P_WIFBLOCKED(this_pcb->status)) {
    // If blocked, waitPID or sleep will already have added the parent to
    // inactive
    pcb* parent = PCBDequeJobSearch(PCBList, this_pcb->parent_pid);
    if (parent != NULL) {
    }
  } else {
  }
}

static void scheduler(void) {
  // The scheduler should not stop for any signal other than SIGALRM
  sigset_t suspend_set;
  sigfillset(&suspend_set);
  sigdelset(&suspend_set, SIGALRM);

  // Register a handler for SIGALRM
  struct sigaction act = (struct sigaction){
      .sa_handler = alarm_handler,
      .sa_mask = suspend_set,
      .sa_flags = SA_RESTART,
  };
  sigaction(SIGALRM, &act, NULL);

  // Make sure SIGALRM is unblocked
  sigset_t alarm_set;
  sigemptyset(&alarm_set);
  sigaddset(&alarm_set, SIGALRM);
  pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);

  struct itimerval it;
  it.it_interval = (struct timeval){.tv_usec = QUANTUM * 1000};
  it.it_value = it.it_interval;
  setitimer(ITIMER_REAL, &it, NULL);

  // Looks to check the global value done
  int choice = 0;
  while (true) {
    ticks++;  // Increment the number of ticks
    k_sleep_check();
    updateplus_pid();
    choice = select_job();

    if (choice == -1) {
      sigsuspend(&suspend_set);
      continue;
    }

    PIDDeque* this_deque = priorityList[choice];
    pid_t threadPID = -1;
    PIDDeque_Peek_Front(this_deque, &threadPID);
    PIDDeque_Pop_Front(this_deque);

    pcb* this_pcb = PCBDequeJobSearch(PCBList, threadPID);
    if (this_pcb == NULL) {
      continue;
    }
    if (threadPID != currentJob) {
      char message[100];
      sprintf(message, "[%3d]\tSCHEDULE \t%d\t%d\t%-15s\n", ticks,
              this_pcb->pid, choice, this_pcb->process_name);
      s_log(message);
    }

    currentJob = threadPID;
    if (!this_pcb->is_background) {
      fgJob = threadPID;
    }

    spthread_t this_thread = this_pcb->curr_thread;
    spthread_continue(this_thread);
    sigsuspend(&suspend_set);
    spthread_suspend(this_thread);
    add_job_back(this_pcb);

    if (logged_out) {
      PCBDeque_Free(PCBList);
      for (int i = 0; i < 4; i++) {
        PIDDeque_Free(priorityList[i]);
      }
      free_history(curr_history);
      exit(EXIT_SUCCESS);
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc == 2) {
    logFileName = "./log/log";
  } else if (argc == 3) {
    logFileName = argv[2];
  } else {
    // Error (incorrect # of arguments)
    P_ERRNO = EARG;
    u_error("Incorrect # of arguments passed in to PennOS");
    exit(EXIT_FAILURE);
  }

  int num_blocks;
  int block_size;

  // Mount the filesystem which is argv[1]
  char* filesystem_filename = argv[1];
  if (mount(filesystem_filename, &num_blocks, &block_size) == -1) {
    P_ERRNO = EARG;
    u_error("Unable to mount provided filesystem to PennOS");
    exit(EXIT_FAILURE);
  }

  // Open logfile
  logfd = open(logFileName, O_TRUNC | O_RDWR | O_CREAT, OPEN_FLAG);

  curr_history = read_history_from_file();

  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTSTP, signal_handler) == SIG_ERR) {
    exit(EXIT_FAILURE);
  }

  // Initialize PCBList and PIDDeques
  k_allocate_lists();

  // Initialize shell process as the base case
  pidCount++;
  pid_t shellPID =
      s_spawn(shell, NULL, STDIN_FILENO, STDOUT_FILENO, "shell", false, NULL);

  s_nice(shellPID, 0);

  scheduler();
}