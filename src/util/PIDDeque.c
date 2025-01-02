#include "PIDDeque.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

// Helper function prototypes (not exposed in header)
static PIDDqNode* createPIDNode(pid_t p);
static void freePIDNode(PIDDqNode* node);

// Deque Operations Implementation
PIDDeque* PIDDeque_Allocate(void) {
  PIDDeque* deque = (PIDDeque*)malloc(sizeof(PIDDeque));
  if (deque) {
    deque->num_elements = 0;
    deque->front = deque->back = NULL;
  }
  return deque;
}

void PIDDeque_Free(PIDDeque* deque) {
  PIDDqNode* current = deque->front;
  while (current) {
    PIDDqNode* next = current->next;
    freePIDNode(current);
    current = next;
  }
  free(deque);
}

int PIDDeque_Size(PIDDeque* deque) {
  return deque->num_elements;
}

void PIDDeque_Push_Front(PIDDeque* deque, pid_t pid) {
  PIDDqNode* node = createPIDNode(pid);
  if (!deque->front) {
    deque->front = deque->back = node;
  } else {
    node->next = deque->front;
    deque->front->prev = node;
    deque->front = node;
  }
  deque->num_elements++;
}

bool PIDDeque_Pop_Front(PIDDeque* deque) {
  if (!deque->front) {
    return false;
  }  // Deque is empty

  PIDDqNode* toDelete = deque->front;
  deque->front = toDelete->next;
  if (deque->front) {
    deque->front->prev = NULL;
  } else {  // Deque became empty
    deque->back = NULL;
  }
  freePIDNode(toDelete);
  deque->num_elements--;
  return true;
}

void PIDDeque_Push_Back(PIDDeque* deque, pid_t payload) {
  PIDDqNode* node = createPIDNode(payload);
  if (!deque->back) {  // Empty deque
    deque->front = deque->back = node;
  } else {
    deque->back->next = node;
    node->prev = deque->back;
    deque->back = node;
  }
  deque->num_elements++;
}

bool PIDDeque_Pop_Back(PIDDeque* deque) {
  if (!deque->back) {
    return false;  // Deque is empty
  }
  PIDDqNode* toDelete = deque->back;
  deque->back = deque->back->prev;
  if (deque->back) {
    deque->back->next = NULL;
  } else {  // Deque became empty
    deque->front = NULL;
  }
  freePIDNode(toDelete);
  deque->num_elements--;
  return true;
}

bool PIDDeque_Peek_Front(PIDDeque* deque, pid_t* pid) {
  if (!deque->front || deque->num_elements == 0) {
    return false;  // Deque is empty
  }
  *pid = (deque->front->pid);
  return true;
}

bool PIDDeque_Peek_Back(PIDDeque* deque, pid_t* pid) {
  if (!deque->back) {
    return false;  // Deque is empty
  }
  *pid = deque->back->pid;
  return true;
}

bool PIDDequeJobSearch(PIDDeque* deque, pid_t pid) {
  PIDDqNode* current = deque->front;  // Start from the front of the deque

  while (current != NULL) {
    if (current->pid == pid) {
      return true;
    }
    current = current->next;
  }
  return false;
}

bool PIDSearchAndDelete(PIDDeque* deque, pid_t pid) {
  PIDDqNode* current = deque->front;
  PIDDqNode* prev = NULL;

  while (current != NULL) {
    if (current->pid == pid) {
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
      freePIDNode(current);
      deque->num_elements--;
      return true;
    }
    prev = current;
    current = current->next;
  }
  return false;  // Node with the specified PID not found
}

// Helper Functions
static PIDDqNode* createPIDNode(pid_t pid) {
  PIDDqNode* node = (PIDDqNode*)malloc(sizeof(PIDDqNode));
  if (node) {
    node->pid = pid;
    node->next = node->prev = NULL;
  }
  return node;
}

static void freePIDNode(PIDDqNode* node) {
  // Free any dynamically allocated memory in the job structure if necessary
  // For example, if you allocate memory for pids or cmd in job, free it here.
  free(node);
}