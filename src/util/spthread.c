#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
// #define _POSIX_C_SOURCE 199309L
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "./spthread.h"

#define MILISEC_IN_NANO 100000

///////////////////////////////////////////////////////////////////////////////
// definitions and  thread_local globals
///////////////////////////////////////////////////////////////////////////////

// function potiner to a function
// that takes a void* and returns a void*
typedef void* (*pthread_fn)(void*);

typedef struct spthread_fwd_args_st {
  // these two are to forward the args of spthread_create so
  // that the function is actually run since spthread_create
  // starts the thread in a setup function
  pthread_fn actual_routine;
  void* actual_arg;

  // these three are to make sure the thread
  // has been created, setup its signal handlers
  // and suspended before spthread_create returns
  // this is so that there is no race condition on
  // sending spthread_suspend or spthread_continue
  // before the thread has setup.
  bool setup_done;
  pthread_mutex_t setup_mutex;
  pthread_cond_t setup_cond;

  // here to send the meta_t to the chlid thread
  spthread_meta_t* child_meta;
} spthread_fwd_args;

// struct used to send signal to spthread
// and so that the thread can set "ack"
// to acknowledge that it is going to stop/continue
typedef struct spthread_signal_args_st {
  const int signal;
  volatile sig_atomic_t ack;
  pthread_mutex_t shutup_mutex;
  // mutex to make helgrind shutup about data races
  // This mutex is probably not necessary, but
  // here for now. Problaby should use <stdatmoic.h>
} spthread_signal_args;

// meta information necessary for
// spthread to work
typedef struct spthread_meta_st {
  // the sigmask when a thread suspends
  sigset_t suspend_set;

  // current state of the spthread
  // Necssary sinnce there may be a
  // race condition on a thread exiting
  // before a signal is sent to it.
  // 0 = normal/running/ready
  // 1 = suspended
  // 2 = exited
  volatile sig_atomic_t state;

  // for data races
  pthread_mutex_t meta_mutex;
} spthread_meta_t;

// Defines the various states
// an spthread is in
#define SPTHREAD_RUNNING_STATE 0
#define SPTHREAD_SUSPENDED_STATE 1
#define SPTHREAD_TERMINATED_STATE 2

// Defined integer values
// that are sent with SIGPTHD
// via sigqueue. Other values are
// safe to send
#define SPTHREAD_SIG_SUSPEND -1
#define SPTHREAD_SIG_CONTINUE -2

// this is a variable that is local to a thread
// (each thread has their own copy of this global)
//
// Will be initialized the thread when created
// points to a heap allocated meta struct
static _Thread_local spthread_meta_t* my_meta = NULL;

///////////////////////////////////////////////////////////////////////////////
// helper declarations
///////////////////////////////////////////////////////////////////////////////

// handler for SIGPTHD to suspend or continue the thrad
static void sigpthd_handler(int signum, siginfo_t*, void*);

// the function that the created thread first runs to
// start off suspended and setup the sigpthd handler
static void* spthread_start(void* arg);

// cleanup function that is automatically invoked on spthread_exit() (or
// pthread_exit). This just makes it so that the thread
// sets itself to be in the "terminated" status
static void mark_self_terminated(void* arg);

///////////////////////////////////////////////////////////////////////////////
// public function definitions
///////////////////////////////////////////////////////////////////////////////

int spthread_create(spthread_t* thread,
                    const pthread_attr_t* attr,
                    pthread_fn start_routine,
                    void* arg) {
  spthread_meta_t* child_meta = malloc(sizeof(spthread_meta_t));
  if (child_meta == NULL) {
    return EAGAIN;
  }

  spthread_fwd_args* fwd_args = malloc(sizeof(spthread_fwd_args));
  if (fwd_args == NULL) {
    free(child_meta);
    return EAGAIN;
  }
  *fwd_args = (spthread_fwd_args){
      .actual_routine = start_routine,
      .actual_arg = arg,
      .setup_done = false,
      .child_meta = child_meta,
  };

  int ret = pthread_mutex_init(&(fwd_args->setup_mutex), NULL);
  if (ret != 0) {
    free(child_meta);
    free(fwd_args);
    return EAGAIN;
  }

  ret = pthread_cond_init(&(fwd_args->setup_cond), NULL);
  if (ret != 0) {
    free(child_meta);
    pthread_mutex_destroy(&(fwd_args->setup_mutex));
    free(fwd_args);
    return EAGAIN;
  }

  pthread_t pthread;
  int result = pthread_create(&pthread, attr, spthread_start, fwd_args);

  pthread_mutex_lock(&(fwd_args->setup_mutex));
  while (fwd_args->setup_done == false) {
    pthread_cond_wait(&(fwd_args->setup_cond), &(fwd_args->setup_mutex));
  }
  pthread_mutex_unlock(&(fwd_args->setup_mutex));

  pthread_cond_destroy(&(fwd_args->setup_cond));
  pthread_mutex_destroy(&(fwd_args->setup_mutex));
  free(fwd_args);

  *thread = (spthread_t){
      .thread = pthread,
      .meta = child_meta,
  };

  return result;
}

int spthread_suspend(spthread_t thread) {
  pthread_t pself = pthread_self();

  if (pthread_equal(pself, thread.thread) != 0) {
    return spthread_suspend_self();
  }

  spthread_signal_args args = (spthread_signal_args){
      .signal = SPTHREAD_SIG_SUSPEND,
      .ack = 0,
  };
  pthread_mutex_init(&args.shutup_mutex, NULL);

  int ret = pthread_sigqueue(thread.thread, SIGPTHD,
                             (union sigval){
                                 .sival_ptr = &args,
                             });
  if (ret != 0) {
    pthread_mutex_destroy(&args.shutup_mutex);
    // handles the case where the thread is already dead.
    return ret;
  }

  // wait for our signal to be ack'd

  // setting up args to nanosleep
  const struct timespec t = (struct timespec){
      .tv_nsec = MILISEC_IN_NANO,
  };

  nanosleep(&t, NULL);

  pthread_mutex_lock(&args.shutup_mutex);
  while (args.ack != 1) {
    // wait for a mili second
    pthread_mutex_unlock(&args.shutup_mutex);

    nanosleep(&t, NULL);

    // fprintf(stderr, "susp checking...\n");
    pthread_mutex_lock(&args.shutup_mutex);

    if (thread.meta->state == SPTHREAD_TERMINATED_STATE) {
      // child called exit, can break
      break;
    }
  }
  pthread_mutex_unlock(&args.shutup_mutex);

  pthread_mutex_destroy(&args.shutup_mutex);
  return ret;
}

int spthread_suspend_self() {
  spthread_t self;
  bool am_sp = spthread_self(&self);
  if (!am_sp) {
    return ESRCH;
  }

  my_meta->state = SPTHREAD_SUSPENDED_STATE;

  do {
    sigsuspend(&my_meta->suspend_set);
  } while (my_meta->state == SPTHREAD_SUSPENDED_STATE);

  return 0;
}

int spthread_continue(spthread_t thread) {
  pthread_t pself = pthread_self();

  if (pthread_equal(pself, thread.thread) != 0) {
    // I am already runnning... so just return 0
    my_meta->state = SPTHREAD_RUNNING_STATE;
    return 0;
  }

  spthread_signal_args args = (spthread_signal_args){
      .signal = SPTHREAD_SIG_CONTINUE,
      .ack = 0,
  };
  pthread_mutex_init(&args.shutup_mutex, NULL);

  int ret = pthread_sigqueue(thread.thread, SIGPTHD,
                             (union sigval){
                                 .sival_ptr = &args,
                             });
  if (ret != 0) {
    pthread_mutex_destroy(&args.shutup_mutex);
    // handles the case where the thread is already dead.
    return ret;
  }

  // wait for our signal to be ack'd

  // setting up args to nanosleep
  const struct timespec t = (struct timespec){
      .tv_nsec = MILISEC_IN_NANO,
  };

  pthread_mutex_lock(&args.shutup_mutex);
  while (args.ack != 1) {
    // wait for a mili second
    pthread_mutex_unlock(&args.shutup_mutex);

    nanosleep(&t, NULL);

    // fprintf(stderr, "susp checking...\n");
    pthread_mutex_lock(&args.shutup_mutex);

    if (thread.meta->state == SPTHREAD_TERMINATED_STATE) {
      // child called exit, can break
      break;
    }
  }
  pthread_mutex_unlock(&args.shutup_mutex);
  pthread_mutex_destroy(&args.shutup_mutex);
  return ret;
}

int spthread_cancel(spthread_t thread) {
  return pthread_cancel(thread.thread);
}

bool spthread_self(spthread_t* thread) {
  if (my_meta == NULL) {
    return false;
  }
  *thread = (spthread_t){
      .thread = pthread_self(),
      .meta = my_meta,
  };
  return true;
}
bool spthread_equal(spthread_t first, spthread_t second) {
  return pthread_equal(first.thread, second.thread) &&
         (first.meta == second.meta);
}
int spthread_join(spthread_t thread, void** retval) {
  int res = pthread_join(thread.thread, retval);
  pthread_mutex_destroy(&thread.meta->meta_mutex);
  free(thread.meta);
  return res;
}

void spthread_exit(void* status) {
  // necessary cleanup is registered
  // in a cleanup routine
  // that is pushed at start of an spthread
  pthread_exit(status);
}

///////////////////////////////////////////////////////////////////////////////
// helper definitions
///////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>

static void sigpthd_handler(int signum,
                            siginfo_t* info,
                            [[maybe_unused]] void* ucontext) {
  //  since suspended is a volatile thread_local primitve,
  //  it should be safe to access from handler
  if (signum != SIGPTHD) {
    // ignore non SIGPTHD signals
    return;
  }

  if (info->si_code == SI_TKILL) {
    // this is an ack message
    return;
  }

  if (info->si_code != SI_QUEUE) {
    char* msg =
        "ERROR: got a sigpthd signal that is not SI_QUEUE or SI_TKILL\nPLEASE "
        "CONTACT COURSE STAFF\n";
    write(STDERR_FILENO, msg, strlen(msg));
    char buf[1024];
    snprintf(buf, 1024, "SI_TKILL %d\tSI_QUEUE %d\tACTUAL %d\n", SI_TKILL,
             SI_QUEUE, info->si_code);
    write(STDERR_FILENO, buf, strlen(buf));
    exit(EXIT_FAILURE);

    return;
  }

  spthread_signal_args* args =
      ((spthread_signal_args*)info->si_value.sival_ptr);
  pthread_mutex_lock(&args->shutup_mutex);
  int s_val = args->signal;

  if (s_val == SPTHREAD_SIG_SUSPEND) {
    my_meta->state = SPTHREAD_SUSPENDED_STATE;
    args->ack = 1;
    pthread_mutex_unlock(&args->shutup_mutex);
    do {
      // man 7 signal-saftey says
      // this function is safe for signal handlers;
      sigsuspend(&my_meta->suspend_set);
    } while (my_meta->state == SPTHREAD_SUSPENDED_STATE);
  } else if (s_val == SPTHREAD_SIG_CONTINUE) {
    my_meta->state = SPTHREAD_RUNNING_STATE;
    args->ack = 1;
    pthread_mutex_unlock(&args->shutup_mutex);
  } else {
    pthread_mutex_unlock(&args->shutup_mutex);
  }
}

static void* spthread_start(void* arg) {
  spthread_fwd_args* args = (spthread_fwd_args*)arg;
  spthread_fwd_args func = *args;
  void* res = NULL;

  // using sigaction so that we can also send a value
  // with sig_queue
  struct sigaction action = {0};  // 0 init (zero out the struct)
  action.sa_sigaction = &sigpthd_handler;
  action.sa_flags = SA_RESTART | SA_SIGINFO;
  sigaction(SIGPTHD, &action, NULL);

  my_meta = args->child_meta;

  sigfillset(&my_meta->suspend_set);
  sigdelset(&my_meta->suspend_set, SIGPTHD);

  pthread_mutex_init(&my_meta->meta_mutex, NULL);

  pthread_cleanup_push(mark_self_terminated, NULL);

  // let spthread_create caller know that
  // we finished setup
  pthread_mutex_lock(&(args->setup_mutex));
  args->setup_done = true;
  my_meta->state = SPTHREAD_SUSPENDED_STATE;
  pthread_cond_broadcast(&(args->setup_cond));
  pthread_mutex_unlock(&(args->setup_mutex));

  // suspend our selves till the scheduler runs us
  do {
    sigsuspend(&my_meta->suspend_set);
  } while (my_meta->state == SPTHREAD_SUSPENDED_STATE);

  // run the desired function
  res = func.actual_routine(func.actual_arg);

  pthread_cleanup_pop(1);

  return res;
}

static void mark_self_terminated(void* arg) {
  // block SIGPTHD to make sure that our code
  // is not suspended during termination
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGPTHD);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);

  my_meta->state = SPTHREAD_TERMINATED_STATE;
}