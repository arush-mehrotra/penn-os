#ifndef PIDDEQUE_H_
#define PIDDEQUE_H_

#include <stdbool.h>  // for bool type (true, false)
#include <sys/types.h>
#include "globals.h"

///////////////////////////////////////////////////////////////////////////////
// A Deque is a Double Ended Queue. We will implement a PID Deque which will
// be used to store the PIDs of the processes for our operating system. There
// will be a separate deque for each priority level in the system.
///////////////////////////////////////////////////////////////////////////////

/** @brief A single node within a deque.
 *
 * A node contains next and prev pointers as well as a pid.
 */
typedef struct pid_dq_node {
  pid_t pid;
  struct pid_dq_node* next;  // next node in deque, or NULL
  struct pid_dq_node* prev;  // prev node in deque, or NULL
} PIDDqNode;

// The entire Deque.
// This struct contains metadata about the deque.
typedef struct dq_struct {
  int num_elements;  //  # elements in the list
  PIDDqNode* front;  // beginning of deque, or NULL if empty
  PIDDqNode* back;   // end of deque, or NULL if empty
} PIDDeque;

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// "Methods" for our Deque implementation.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

/** @brief Allocates and returns a pointer to a new Deque.
 *
 * It is the Caller's responsibility to at some point call Deque_Free to free
 * the associated memory.
 *
 * @return the newly-allocated deque, or NULL on error.
 */
PIDDeque* PIDDeque_Allocate(void);

/** @brief Free a Deque that was previously allocated by Deque_Allocate.
 *
 * @param deque the deque to free. It is unsafe to use "deque" after this
 * function returns.
 */
void PIDDeque_Free(PIDDeque* deque);

/** @brief Return the number of elements in the deque.
 *
 * @param deque the deque to query.
 * @return deque size.
 */
int PIDDeque_Size(PIDDeque* deque);

/** @brief Adds a new element to the front of the Deque.
 *
 * @param deque the Deque to push onto.
 * @param payload the payload to push to the front
 */
void PIDDeque_Push_Front(PIDDeque* deque, pid_t payload);

/** @brief Pop an element from the front of the deque.
 *
 * @param deque the Deque to pop from.
 * @param payload_ptr a return parameter; on success, the popped node's payload
 * is returned through this parameter.
 * @return false on failure (e.g., the deque is empty), true on success.
 */
bool PIDDeque_Pop_Front(PIDDeque* deque);

/** @brief Peeks at the element at the front of the deque.
 *
 * @param deque the Deque to peek.
 * @param payload_ptr a return parameter; on success, the peeked node's payload
 * is returned through this parameter.
 * @return false on failure (e.g., the deque is empty), true on success.
 */
bool PIDDeque_Peek_Front(PIDDeque* deque, pid_t* payload_ptr);

/** @brief Pushes a new element to the end of the deque.
 *
 * @param deque the Deque to push onto.
 * @param payload the payload to push to the end
 */
void PIDDeque_Push_Back(PIDDeque* deque, pid_t payload);

/** @brief Pops an element from the end of the deque.
 *
 * @param deque the Deque to remove from
 * @param payload_ptr a return parameter; on success, the popped node's payload
 * is returned through this parameter.
 * @return false on failure (e.g., the deque is empty), true on success.
 */
bool PIDDeque_Pop_Back(PIDDeque* deque);

/** @brief Peeks at the element at the back of the deque.
 *
 * @param deque the Deque to peek.
 * @param payload_ptr a return parameter; on success, the peeked node's payload
 * is returned through this parameter.
 * @return false on failure (e.g., the deque is empty), true on success.
 */
bool PIDDeque_Peek_Back(PIDDeque* deque, pid_t* pid);

/** @brief Searches for a PID in the deque.
 *
 * @param deque the Deque to search within.
 * @param pid the PID to search for.
 * @return true if the PID is found, false otherwise.
 */
bool PIDDequeJobSearch(PIDDeque* deque, pid_t pid);

/** @brief Searches for a PID in the deque and deletes it if found.
 *
 * @param deque the Deque to modify.
 * @param pid the PID to search and delete.
 * @return true if the PID was successfully deleted, false otherwise.
 */
bool PIDSearchAndDelete(PIDDeque* deque, pid_t pid);

extern PIDDeque* priorityList[4];
#endif
