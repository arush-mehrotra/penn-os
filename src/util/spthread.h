#ifndef SPTHREAD_H_
#define SPTHREAD_H_

#include <pthread.h>
#include <stdbool.h>

// CAUTION: according to `man 7 pthread`:
//
//   On older Linux kernels, SIGUSR1 and SIGUSR2
//   are used.  Applications must avoid the use of whichever set of
//   signals is employed by the implementation.
//
// This may not work on other linux versions

// SIGNAL PTHREAD
// NOTE: if within a created spthread you change
// the behaviour of SIGUSR1, then you will not be able
// to suspend and continue a spthread
#define SIGPTHD SIGUSR1

// declares a struct, but the internals of the
// struct cannot be seen by functions outside of spthread.c
typedef struct spthread_meta_st spthread_meta_t;

// The spthread wrapper struct.
// Sometimes you may have to access the inner pthread member
// but you shouldn't need to do that
typedef struct spthread_st {
  pthread_t thread;
  spthread_meta_t* meta;
} spthread_t;

// NOTE:
// None of these are signal safe
// Also note that most of these functions are not safe to suspension,
// meaning that if the thread calling these is an spthread and is suspended
// in the middle of spthread_continue or spthread_suspend, then it may not work.
//
// Make sure that the calling thread cannot be suspended before calling these
// functions. Exceptions to this are spthread_exit(), spthread_self() and if a
// thread is continuing or suspending itself.

// spthread_create:
// this function works similar to pthread_create, except for two differences.
// 1) the created pthread is able to be asychronously suspended, and continued
//    using the functions:
//      - spthread_suspend
//      - spthread_continue
// 2) The created pthread will be suspended before it executes the specified
//    routine. It must first be continued with `spthread_continue` before
//    it will start executing.
//
// It is worth noting that this function is not signal safe.
// In other words, it should not be called from a signal handler.
//
// to avoid repetition, see pthread_create(3) for details
// on arguments and return values as they are the same here.
int spthread_create(spthread_t* thread,
                    const pthread_attr_t* attr,
                    void* (*start_routine)(void*),
                    void* arg);

// The spthread_suspend function will signal to the
// specified thread to suspend execution.
//
// Calling spthread_suspend on an already suspended
// thread does not do anything.
//
// It is worth noting that this function is not signal safe.
// In other words, it should not be called from a signal handler.
//
// args:
// - pthread_t thread: the thread we want to suspend
//   This thread must be created using the spthread_create() function,
//   if created by some other function, the behaviour is undefined.
//
// returns:
// - 0 on success
// - EAGAIN if the thread could not be signaled
// - ENOSYS if not supported on this system
// - ESRCH if the thread specified is not a valid pthread
int spthread_suspend(spthread_t thread);

// The spthread_suspend_self function will cause the calling
// thread (which should be created by spthread_create) to suspend
// itself.
//
// returns:
// - 0 on success
// - EAGAIN if the thread could not be signaled
// - ENOSYS if not supported on this system
// - ESRCH if the calling thread is not an spthread
int spthread_suspend_self();

// The spthread_continue function will signal to the
// specified thread to resume execution if suspended.
//
// Calling spthread_continue on an already non-suspended
// thread does not do anything.
//
// It is worth noting that this function is not signal safe.
// In other words, it should not be called from a signal handler.
//
// args:
// - spthread_t thread: the thread we want to continue
//   This thread must be created using the spthread_create() function,
//   if created by some other function, the behaviour is undefined.
//
// returns:
// - 0 on success
// - EAGAIN if the thread could not be signaled
// - ENOSYS if not supported on this system
// - ESRCH if the thread specified is not a valid pthread
int spthread_continue(spthread_t thread);

// The spthread_cancel function will send a
// cancellation request to the specified thread.
//
// as of now, this function is identical to pthread_cancel(3)
// so to avoid repitition, you should look there.
//
// Here are a few things that are worth highlighting:
// - it is worth noting that it is a cancellation __request__
//   the thread may not terminate immediately, instead the
//   thread is checked whenever it calls a function that is
//   marked as a cancellation point. At those points, it will
//   start the cancellation procedure
// - to make sure all things are de-allocated properly on
//   normal exiting of the thread and when it is cancelled,
//   you should mark a defered de-allocation with
//   pthread_cleanup_push(3).
//   consider the following example:
//
//     void* thread_routine(void* arg) {
//        int* num = malloc(sizeof(int));
//        pthread_cleanup_push(&free, num);
//        return NULL;
//     }
//
//    this program will allocate an integer on the heap
//    and mark that data to be de-allocated on cleanup.
//    This means that when the thread returns from the
//    routine specified in spthread_create, free will
//    be called on num. This will also happen if the thread
//    is cancelled and not able to be exited normally.
//
//    Another function that should be used in conjunction
//    is pthread_cleanup_pop(3). I will leave that
//    to you to read more on.
//
// It is worth noting that this function is not signal safe.
// In other words, it should not be called from a signal handler.
//
// args:
// - spthread_t thread: the thread we want to cancel.
//   This thread must be created using the spthread_create() function,
//   if created by some other function, the behaviour is undefined.
//
// returns:
// - 0 on success
// - ESRCH if the thread specified is not a valid pthread
int spthread_cancel(spthread_t thread);

bool spthread_equal(spthread_t first, spthread_t second);

// Can be called by a thread to get two peices of information:
// 1. Whether or not the calling thread is an spthread (true or false)
// 2. The spthread_t of the calling thread, if it is an spthread_t
//
// almost always the function will be called like this:
// spthread_t self;
// bool i_am_spthread = spthread_self(&self);
//
// args:
// - spthread_t* thread: the output parameter to get the spthread_t
//   representing the calling thread, if it is an spthread
//
// returns:
// - true if the calling thread is an spthread_t
// - false otherwise.
bool spthread_self(spthread_t* thread);

// The equivalent of pthread_join but for spthread
// To make sure all resources are cleaned up appropriately
// spthreads that are created must at some ppoint have spthread_join
// called on them. Do not use pthread_join on an spthread.
//
// to avoid repetition, see pthread_join(3) for details
// on arguments and return values as they are the same as this function.
int spthread_join(spthread_t thread, void** retval);

// The equivalent of pthread_exit but for spthread
// spthread_exit must be used by spthreads instead of pthread_exit.
// Otherwise, calls to spthread_join or other functions (like spthread_suspend)
// may not work as intended.
//
// to avoid repetition, see pthread_exit(3) for details
// on arguments and return values as they are the same as this function.
void spthread_exit(void* status);

#endif  // SPTHREAD_H_