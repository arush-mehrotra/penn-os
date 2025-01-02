#include "PCBDeque.h"
#include <stdio.h>
#include <stdlib.h>
#include "PCB.h"
#include "PIDDeque.h"
#include "globals.h"

// Helper function prototypes (not exposed in header)
static PCBDqNode* createNode(pcb* payload);
static void freeNode(PCBDqNode* node);

// Deque Operations Implementation
PCBDeque* PCBDeque_Allocate(void) {
  PCBDeque* deque = (PCBDeque*)malloc(sizeof(PCBDeque));
  if (deque) {
    deque->num_elements = 0;
    deque->front = deque->back = NULL;
  }
  return deque;
}

void PCBDeque_Free(PCBDeque* deque) {
  PCBDqNode* current = deque->front;
  while (current) {
    PCBDqNode* next = current->next;
    freeNode(current);
    current = next;
  }
  free(deque);
}

int PCBDeque_Size(PCBDeque* deque) {
  return deque->num_elements;
}

void PCBDeque_Push_Front(PCBDeque* deque, pcb* payload) {
  PCBDqNode* node = createNode(payload);
  if (!deque->front) {
    deque->front = deque->back = node;
  } else {
    node->next = deque->front;
    deque->front->prev = node;
    deque->front = node;
  }
  deque->num_elements++;
}

bool PCBDeque_Pop_Front(PCBDeque* deque) {
  if (!deque->front) {
    return false;
  }  // Deque is empty

  PCBDqNode* toDelete = deque->front;
  deque->front = deque->front->next;
  if (deque->front) {
    deque->front->prev = NULL;
  } else {  // Deque became empty
    deque->back = NULL;
  }
  freeNode(toDelete);
  deque->num_elements--;
  return true;
}

void PCBDeque_Push_Back(PCBDeque* deque, pcb* payload) {
  PCBDqNode* node = createNode(payload);
  if (!deque->back) {  // Empty deque
    deque->front = deque->back = node;
  } else {
    deque->back->next = node;
    node->prev = deque->back;
    deque->back = node;
  }
  deque->num_elements++;
}

bool PCBDeque_Pop_Back(PCBDeque* deque) {
  if (!deque->back) {
    return false;  // Deque is empty
  }
  PCBDqNode* toDelete = deque->back;
  deque->back = deque->back->prev;
  if (deque->back) {
    deque->back->next = NULL;
  } else {  // Deque became empty
    deque->front = NULL;
  }
  freeNode(toDelete);
  deque->num_elements--;
  return true;
}

bool PCBDeque_Peek_Front(PCBDeque* deque, pcb* payload_ptr) {
  if (!deque->front) {
    return false;  // Deque is empty
  }
  payload_ptr = (deque->front->pcb);
  return true;
}

bool PCBDeque_Peek_Back(PCBDeque* deque, pcb** payload_ptr) {
  if (!deque->back) {
    return false;  // Deque is empty
  }
  *payload_ptr = deque->back->pcb;
  return true;
}

pcb* PCBDequeJobSearch(PCBDeque* deque, pid_t job_id) {
  PCBDqNode* current = deque->front;  // Start from the front of the deque
  while (current != NULL) {
    if (current->pcb->pid == job_id) {
      return (current->pcb);
    }
    current = current->next;
  }
  return NULL;
}

bool PCBSearchAndDelete(PCBDeque* deque, pid_t pid, bool shouldFreeNode) {
  PCBDqNode* current = deque->front;
  PCBDqNode* prev = NULL;

  while (current != NULL) {
    if (current->pcb->pid == pid) {
      // Found the node with the matching pid
      if (prev) {
        prev->next = current->next;
      } else {
        // We're removing the front node
        deque->front = current->next;
      }

      if (current->next) {
        current->next->prev = prev;
      } else {
        // We're removing the back node
        deque->back = prev;
      }
      if (shouldFreeNode) {
        freeNode(current);
      } else {
        free(current);
      }
      deque->num_elements--;
      return true;
    }
    prev = current;
    current = current->next;
  }
  return false;  // Node with the specified PID not found
}

pcb* PCBDequeStopSearch(PCBDeque* deque) {
  PCBDqNode* current = deque->front;
  pcb* maxStopTimeJob = NULL;
  int maxStopTime = 0;

  while (current != NULL) {
    if (current->pcb->stop_time > maxStopTime) {
      maxStopTime = current->pcb->stop_time;  // Update the maxStopTime
      maxStopTimeJob = (current->pcb);        // Update the pointer to the job
                                              // with the largest stop_time
    }
    current = current->next;  // Move to the next node in the deque
  }
  return maxStopTimeJob;  // Return the job with the largest non-zero stop_time,
                          // or NULL if none found
}
// Helper Functions
static PCBDqNode* createNode(pcb* payload) {
  PCBDqNode* node = (PCBDqNode*)malloc(sizeof(PCBDqNode));
  if (node) {
    node->pcb = payload;
    node->next = node->prev = NULL;
  }
  return node;
}

static void freePCB(pcb* pcb) {
  // Free any dynamically allocated memory in the job structure if necessary
  // For example, if you allocate memory for pids or cmd in job, free it here.
  PIDDeque_Free((pcb->child_pids));
  PIDDeque_Free((pcb->status_changes));
  if (pcb->parsed != NULL) {
    free(pcb->parsed);
  }
  spthread_cancel(pcb->curr_thread);
  spthread_continue(pcb->curr_thread);
  spthread_join(pcb->curr_thread, NULL);
  free(pcb);
}

static void freeNode(PCBDqNode* node) {
  // Free any dynamically allocated memory in the job structure if necessary
  // For example, if you allocate memory for pids or cmd in job, free it here.
  freePCB(node->pcb);
  free(node);
}

pcb* PCBDequeBackgroundSearch(PCBDeque* deque) {
  PCBDqNode* current = deque->front;
  pcb* recentlyBG = NULL;

  while (current != NULL) {
    if (current->pcb->is_background) {
      recentlyBG = current->pcb;
    }
    current = current->next;  // Move to the next node in the deque
  }
  return recentlyBG;  // Return the job with the largest non-zero stop_time,
                      // or NULL if none found
}