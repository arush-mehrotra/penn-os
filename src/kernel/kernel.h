#ifndef KERNEL_H
#define KERNEL_H
#include <stdlib.h>
#include "../fat/fat_helper.h"
#include "../util/PCB.h"
#include "../util/PCBDeque.h"
#include "../util/PIDDeque.h"
#include "../util/globals.h"
#include "../util/macros.h"
#include "../util/spthread.h"

char* command_print_helper(char*** commands);
void k_allocate_lists(void);  // allocates all deques

/**
 * @brief Create a new child process, inheriting applicable properties from the
 * parent.
 *
 * @return Reference to the child PCB.
 */
pcb* k_proc_create(pcb* parent,
                   spthread_t thread,
                   int fd0,
                   int fd1,
                   char* process_name,
                   bool is_background,
                   struct parsed_command* parsed);

/**
 * @brief Get the PCB of the currently running process.
 *
 * @return Reference to the child PCB.
 */
pcb* k_get_proc(void);

/**
 * @brief Sends a signal to a PID
 *
 * @return Returns 0 on success, -1 on failure.
 */
int k_send_signal(pid_t pid, int signal);

/**
 * @brief Change priority of a pid to given priority
 *
 * @return Returns 0 on success, -1 on failure.
 */
int k_change_priority(pid_t pid, int priority);

/**
 * @brief Wait on child of the calling process.
 *
 * @return Returns pid_of child, -1 on failure.
 */
pid_t k_waitpid(pid_t pid, int* wstatus, bool nohang);

/**
 * @brief Exits out of the calling process. Doesn't clean up the process
 *
 * @return nothing
 */
void k_exit(void);

/**
 * @brief Sleeps for ticks amount of time
 *
 * @return nothing
 */
void k_sleep(unsigned int ticks);

/**
 * @brief Clean up a terminated/finished thread's resources.
 * This may include freeing the PCB, handling children, etc.
 */
void k_proc_cleanup(pcb* proc);

/**
 * @brief Checks for all sleeping and running jobs. Maintains a counter for how
 * long they have been sleeping for. Once a slept job has finished running,
 * change the status and informs the parent, scheduling the parent if necessary.
 * @return nothing
 */
void k_sleep_check(void);

/**
 * @brief Helper function called within waitpid, used to handle a status change
 * of the job specified in target_pid, calls k_proc_cleanup if necessary.
 * @return nothing
 */
void k_handle_status_changes(pid_t target_pid);

/**
 * @brief Function which writes status updates to the log file.
 * @return nothing
 */
void k_write_log(char* message);

/**
 * @brief Kernel function which handles the ps job command.
 * @return nothing
 */
void k_ps();

/**
 * @brief Helper function which maps the status of a job to a char used for
 * logging and job status updates
 * @return nothing
 */
char* get_status(int status);

/**
 * @brief Function which handles the 'bg' command on the specified pid. If no
 * pid is provided to bg, the input to k_handle_bg is -1.
 * @return 0 on success, -1 on error
 */
int k_handle_bg(pid_t pid);

/**
 * @brief Function which handles the 'fg' command on the specified pid. If no
 * pid is provided to fg, the input to k_handle_fg is -1.
 * @return 0 on success, -1 on error
 */
int k_handle_fg(pid_t pid);

#endif  // KERNEL_SYSTEM_H
