# PennKeys
apatwa, zeinje, arushm, nagwekar

# List of submitted source files:
- src/fat/fat_globals.h
- src/fat/fat_helper.h
- src/fat/fat_helper.c
- src/kernel/kernel_system.h
- src/kernel/kernel_system.c
- src/kernel/kernel.h
- src/kernel/kernel.c
- src/kernel/shell.h
- src/kernel/shell.c
- src/kernel/stress.h
- src/kernel/stress.c
- src/util/builtins.h
- src/util/builtins.c
- src/util/globals.h
- src/util/macros.h
- src/util/os_errors.h
- src/util/os_errors.c
- src/util/parser.h
- src/util/PCB.h
- src/util/PCBDeque.h
- src/util/PCBDeque.c
- src/util/PIDDeque.h
- src/util/PIDDeque.c
- src/util/spthread.h
- src/util/spthread.c
- src/pennfat.c
- src/pennos.c

# Extra credit answers
We are going for Valgrind extra credit -- no memory errors or leaks.
We also implemented noncanonical mode for the shell (specifically, supporting up and down arrows for command history)

# Compilation Instructions
- Navigate to the root directory.
- Ensure that there is a folder called "log" -- if not, run `mkdir log`.
- Run `make`
- Run `./bin/pennfat`
- In the prompt, run `mkfs minfs 1 0` or whatever configuration you desire.
- Exit PennFAT
- Run `./bin/pennos pennfat`

# Overview of work accomplished
We have successfully built a single-core operating system, with a FAT-based filesystem, a kernel, and a scheduler that correctly decides which processes to run. We have preserved the necessary abstractions between kernel, system, and user land. We have implemented a number of builtin functions that can be run from our shell and interact with the filesystem. We have tested the functionality of the entire system, including the correct CPU utilization and memory leaks.

# Description of code and code layout
src/fat contains all of the internal code that interacts with the FAT. src/pennfat.c contains the main function from which user input is taken, and the FAT is actually built.

src/kernel contains the kernel and system level functions that do operations like: spawn threads, change priorities, wait on jobs, as well as run all of the builtins. The kernel functions are in kernel.c and the system-level functions, many of which call kernel functions, are in kernel_system.c. src/kernel also contains the code for the shell in shell.c, which contains the main loop that prompts, takes user input, and then spawns children threads for builtins.

src/util contains the bulk of the helpers. Builtins.c contain the functions that are actually run inside of the child threads spawned by the shell. Globals.h contains the global externs we use across the project. Macros.h contains constants for signal codes. Os_errors.c contains code for custom error handling. PCBDeque.c and PIDDeque.c contain the implementations of the deques we use to store PCB information, and to handle the scheduling of jobs. PCB.h contains the definition of the PCB struct.

Finally, pennos.c is the main PennOS function that spawns the shell and runs the scheduler.

# Description of PCB struct
The PCB struct has the following fields:
pid_t pid: the process id, incremented each time a new process is spawned
int status: a code for the status of the process
pid_t parent_pid: parent process id, -1 if no parent
spthread_t curr_thread: the thread that this PCB is running
PIDDeque* child_pids: a list of the PIDs of this process' children
PIDDeque* status_changes: a list of all of the PIDs that have seen their status update
int blocking: 1 if blocking, 0 if not
int priority: priority level between 0 and 2
int sleep_duration; number of quanta to sleep for. If not sleeping, set sleep_duration = -1;
char* process_name: name of process
int stop_time: when it was stopped
bool is_background: is it in the background
int process_fdt[1024]: process-level file descriptor table
struct parsed_command* parsed: the command corresponding to this process
int job_id: used for storing JobID





