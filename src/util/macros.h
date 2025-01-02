
// SIGNAL MACROS
#define P_SIGSTOP 69
#define P_SIGCONT 70
#define P_SIGTERM 71

// STATUS MACROS
#define STATUS_RUNNING 100
#define STATUS_STOPPED 101
#define STATUS_BLOCKED 102
#define STATUS_FINISHED 103
#define STATUS_TERMINATED 104

#define P_WIFRUNNING(status) (status == STATUS_RUNNING ? 1 : 0)
#define P_WIFSTOPPED(status) (status == STATUS_STOPPED ? 1 : 0)
#define P_WIFBLOCKED(status) (status == STATUS_BLOCKED ? 1 : 0)
#define P_WIFEXITED(status) (status == STATUS_FINISHED ? 1 : 0)
#define P_WIFSIGNALED(status) (status == STATUS_TERMINATED ? 1 : 0)