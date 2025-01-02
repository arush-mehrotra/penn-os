#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <unistd.h>
#include "./terminal_history.h"

extern pid_t pidCount;  // global variable which assigns PID to new process,
                        // incremented by one each time
extern pid_t currentJob;

extern pid_t fgJob;

extern int ticks;

extern char* logFileName;

extern int logfd;

extern bool logged_out;

extern int num_bg_jobs;

extern pid_t plus_pid;

extern TerminalHistory* curr_history;
#endif