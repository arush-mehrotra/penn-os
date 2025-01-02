#ifndef PCBDeque_H_
#define PCBDeque_H_

#include <stdbool.h>  // for bool type (true, false)
#include "PCB.h"
#include "globals.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

// A single node within a deque.
//
// A node contains next and prev pointers as well as a pointer to a PCB struct.
typedef struct pcb_dq_node {
  pcb* pcb;                  // Stores PCB
  struct pcb_dq_node* next;  // next node in deque, or NULL
  struct pcb_dq_node* prev;  // prev node in deque, or NULL
} PCBDqNode;

// The entire Deque.
// This struct contains metadata about the deque.
typedef struct dq_st {
  int num_elements;  //  # elements in the list
  PCBDqNode* front;  // beginning of deque, or NULL if empty
  PCBDqNode* back;   // end of deque, or NULL if empty
} PCBDeque;

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// "Methods" for our Deque implementation.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

/**
 * @brief Allocates and returns a pointer to a new Deque.
 *
 * @return pointer to the deque, NULL on error
 */
PCBDeque* PCBDeque_Allocate(void);

/**
 * @brief Free a Deque that was previously allocated by Deque_Alocate
 *
 * @param deque: the deque pointer to free. Will be unsafe to use after.
 */
void PCBDeque_Free(PCBDeque* deque);

/**
 * @brief Return the number of elements in the Deque.
 *
 * @param deque: the pointer to the deque we are measuring
 * @return pointer to the deque, NULL on error
 */
int PCBDeque_Size(PCBDeque* deque);

/**
 * @brief Add a new element to the front of the Deque.
 *
 * @param deque: the pointer to the Deque we are adding to
 * @param payload: the pointer to the struct we are adding
 */
void PCBDeque_Push_Front(PCBDeque* deque, pcb* payload);

/**
 * @brief Pop an element from the front of the Deque.
 *
 * @param deque: the deque that we are popping from
 * @return true on success, false if the deque is empty or error
 */
bool PCBDeque_Pop_Front(PCBDeque* deque);

/**
 * @brief Peek at the element at the front of the deque.
 *
 * @param deque: the deque we are peeking at
 * @param payload_ptr: a return parameter; a pointer that will point to the
 * peeked struct
 * @return true on success, false if the deque is empty or error
 */
bool PCBDeque_Peek_Front(PCBDeque* deque, pcb* payload_ptr);

/**
 * @brief Add a new element to the back of the Deque.
 *
 * @param deque: the pointer to the Deque we are adding to
 * @param payload: the pointer to the struct we are adding
 */
void PCBDeque_Push_Back(PCBDeque* deque, pcb* payload);

/**
 * @brief Pop an element from the back of the Deque.
 *
 * @param deque: the deque that we are popping from
 * @return true on success, false if the deque is empty or error
 */
bool PCBDeque_Pop_Back(PCBDeque* deque);

/**
 * @brief Peek at the element at the back of the deque.
 *
 * @param deque: the deque we are peeking at
 * @param payload_ptr: a return parameter; a pointer that will point to the
 * peeked struct
 * @return true on success, false if the deque is empty or error
 */
bool PCBDeque_Peek_Back(PCBDeque* deque, pcb** payload_ptr);

/**
 * @brief Search the Deque from a struct containing a certain process/jobID
 *
 * @param deque: the deque to search inside
 * @param job_id: the pid that we are looking for
 * @return A pointer to an element in the deque that matches; NULL if no match
 * found
 */
pcb* PCBDequeJobSearch(PCBDeque* deque, int job_id);

/**
 * @brief Search the Deque from a struct containing a certain process/jobID and
 * delete it
 *
 * @param deque: the deque to search inside
 * @param pgid: the pid that we are looking for
 * @param shouldFreeNode: should we also free the memory allocated for this pcb
 * struct
 * @return true if successfully deleted, false if no match was found or error
 */
bool PCBSearchAndDelete(PCBDeque* deque, pid_t pgid, bool shouldFreeNode);

/**
 * @brief Search a Deque for the first job with "STOPPED" status
 *
 * @param deque: the deque to search inside
 * @return A pointer to an element in the deque that matches; NULL if no match
 * found
 */
pcb* PCBDequeStopSearch(PCBDeque* deque);

/**
 * @brief Search a Deque for the first job that is in the background
 *
 * @param deque: the deque to search inside
 * @return A pointer to an element in the deque that matches; NULL if no match
 * found
 */
pcb* PCBDequeBackgroundSearch(PCBDeque* deque);

extern PCBDeque* PCBList;  // make PCBList a global var

#endif
