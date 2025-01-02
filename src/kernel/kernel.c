#include "kernel.h"
#include <stdio.h>
#include <string.h>
#include "../util/parser.h"

char* command_print_helper(char*** commands) {
  if (commands == NULL || *commands == NULL) {
    return NULL;
  }

  static char result[1024];  // Fixed-size buffer
  char** firstRow = *commands;
  result[0] = '\0';  // Initialize the buffer to an empty string
  size_t currentLength = 0;

  // Iterate over each string in the first row
  for (int i = 0; firstRow[i] != NULL; i++) {
    size_t len = strlen(firstRow[i]);

    // Check if adding this string and a space (if not the last element) would
    // overflow the buffer
    if (currentLength + len + 1 >= sizeof(result)) {
      return NULL;
    }

    // Append the current string to the result
    strcat(result, firstRow[i]);
    currentLength += len;

    // Add a space if this is not the last string and there's space
    if (firstRow[i + 1] != NULL && currentLength + 1 < sizeof(result)) {
      strcat(result, " ");
      currentLength++;
    }
  }

  return result;
}

void k_allocate_lists() {
  PCBList = PCBDeque_Allocate();
  for (int i = 0; i < 4; i++) {
    priorityList[i] = PIDDeque_Allocate();
  }
}

// Helper to intialize fdt for process to relevant values
void initialize_fdt(pcb* proc, int file_in, int file_out) {
  proc->process_fdt[0] = file_in;
  proc->process_fdt[1] = file_out;
  for (int i = 2; i < 1024; i++) {
    proc->process_fdt[i] = -1;
  }
}

pcb* k_proc_create(pcb* parent,
                   spthread_t curr_thread,
                   int fd0,
                   int fd1,
                   char* process_name,
                   bool is_background,
                   struct parsed_command* parsed) {
  // create child PCB
  pcb* child = malloc(sizeof(pcb));
  child->pid = pidCount;
  child->status = STATUS_RUNNING;
  child->parent_pid = parent != NULL ? parent->pid : -1;
  child->curr_thread = curr_thread;
  child->child_pids = PIDDeque_Allocate();
  child->status_changes = PIDDeque_Allocate();
  child->priority = 1;
  child->blocking = is_background ? 0 : 1;
  child->sleep_duration = -1;
  child->process_name = process_name;
  child->stop_time = -1;
  child->is_background = is_background;
  child->parsed = parsed;
  child->job_id = 0;
  initialize_fdt(child, fd0, fd1);

  // include child PCB in child_pids
  if (parent != NULL) {
    PIDDeque_Push_Back(parent->child_pids, child->pid);
  }
  // put in prioirty list
  PIDDeque_Push_Back(priorityList[child->priority], child->pid);
  PCBDeque_Push_Back(PCBList, child);
  // update global PID Count
  pidCount++;

  // print out job number and pid in [1] 34234 format
  if (is_background && child->parent_pid == 1) {
    char announcement[1024];
    child->job_id = ++num_bg_jobs;
    sprintf(announcement, "[%d] %d\n", child->job_id, child->pid);
    k_write(1, announcement, strlen(announcement));
  }
  return child;
}

pcb* k_get_proc() {
  return PCBDequeJobSearch(PCBList, currentJob);
}

int k_send_signal(pid_t pid, int signal) {  // if parent is killed, should
                                            // automatically reap children ???
  // find pid in PCBList, if doesn't exist, return -1
  pcb* proc = PCBDequeJobSearch(PCBList, pid);
  if (proc == NULL) {
    return -1;
  }
  int prevStatus = proc->status;
  // Signal cannot be sent to a finished or terminated job
  if (P_WIFEXITED(prevStatus) || P_WIFSIGNALED(prevStatus)) {
    return -1;
  }
  // Set new status
  int newStatus;
  if (signal == P_SIGCONT) {
    newStatus = STATUS_RUNNING;
    if (strcmp(proc->process_name, "sleep") == 0) {
      newStatus = STATUS_BLOCKED;
    }
    char message[100];
    sprintf(message, "[%3d]\tCONTINUED\t%d\t%d\t%-15s\n", ticks, proc->pid,
            proc->priority, proc->process_name);
    k_write_log(message);
    if (proc->parent_pid == 1) {
      char announcement[1024];
      char plus = (proc->pid == plus_pid) ? '+' : ' ';
      sprintf(announcement, "[%d]%c %d continued %s\n", proc->job_id, plus,
              proc->pid, command_print_helper((proc->parsed)->commands));
      k_write(STDOUT_FILENO, announcement, strlen(announcement));
    }
  } else if (signal == P_SIGSTOP) {
    newStatus = STATUS_STOPPED;
    proc->stop_time = ticks;
    proc->is_background = true;
    if (proc->parent_pid == 1) {
      if (proc->job_id == 0) {
        proc->job_id = ++num_bg_jobs;
      }
      char announcement[1024];
      sprintf(announcement, "[%d]+ %d suspended %s\n", proc->job_id, proc->pid,
              command_print_helper((proc->parsed)->commands));
      k_write(1, announcement, strlen(announcement));
    }
    // move recently stopped jobs to back of list;
    PCBSearchAndDelete(PCBList, proc->pid, false);
    PCBDeque_Push_Back(PCBList, proc);

    char message[100];
    sprintf(message, "[%3d]\tSTOPPED  \t%d\t%d\t%-15s\n", ticks, proc->pid,
            proc->priority, proc->process_name);
    k_write_log(message);
  } else if (signal == P_SIGTERM) {
    newStatus = STATUS_TERMINATED;
    char message[100];
    sprintf(message, "[%3d]\tSIGNALED \t%d\t%d\t%-15s\n", ticks, proc->pid,
            proc->priority, proc->process_name);
    k_write_log(message);

    if (proc->parent_pid != -1) {
      char message[100];
      sprintf(message, "[%3d]\tZOMBIE   \t%d\t%d\t%-15s\n", ticks, proc->pid,
              proc->priority, proc->process_name);
      k_write_log(message);
    }

    PIDDeque* children = proc->child_pids;
    PIDDqNode* head = children->front;
    while (head != NULL) {
      pcb* child_proc = PCBDequeJobSearch(PCBList, head->pid);
      if (child_proc == NULL) {
        head = head->next;
        continue;
      }
      char message[100];
      sprintf(message, "[%3d]\tORPHAN   \t%d\t%d\t%-15s\n", ticks,
              child_proc->pid, child_proc->priority, child_proc->process_name);
      k_write_log(message);
      break;
    }

    if (proc->is_background && proc->parent_pid == 1) {
      char announcement[1024];
      // fill message up with each process id info, inshallah it does not
      // overflow
      char plus = (proc->pid == plus_pid) ? '+' : ' ';
      sprintf(announcement, "[%d]%c %d terminated %s\n", proc->job_id, plus,
              proc->pid, command_print_helper((proc->parsed)->commands));
      k_write(STDOUT_FILENO, announcement, strlen(announcement));
    }
  }

  // status change for PID
  if (newStatus != prevStatus) {
    // update status of current pid to signal value
    proc->status = newStatus;
    // previously a suspended, waiting, or
    // stopped process (in priorityList[3])
    if (P_WIFRUNNING(newStatus)) {
      PIDSearchAndDelete(priorityList[3], pid);
      PIDDeque_Push_Back(priorityList[proc->priority], pid);
      // previously running, now stopped or terminated
    } else if ((P_WIFSTOPPED(newStatus) || P_WIFSIGNALED(newStatus)) &&
               !PIDDequeJobSearch(priorityList[3], pid)) {
      PIDSearchAndDelete(priorityList[proc->priority], pid);
      PIDDeque_Push_Back(priorityList[3], pid);
    }
    // update parent's status_change deque with recent status change
    pcb* parent = PCBDequeJobSearch(PCBList, proc->parent_pid);
    PIDDeque_Push_Back(parent->status_changes, pid);

    // update parent if i was blocking and am now running (parent should no
    // longer be inactive)
    if (proc->blocking && !P_WIFRUNNING(proc->status)) {
      if (proc->status == STATUS_STOPPED) {
        proc->blocking = false;
      }
      parent->status = STATUS_RUNNING;
      PIDSearchAndDelete(priorityList[3], parent->pid);
      PIDDeque_Push_Back(priorityList[parent->priority], parent->pid);
      char message[100];
      sprintf(message, "[%3d]\tUNBLOCKED\t%d\t%d\t%-15s\n", ticks, parent->pid,
              parent->priority, parent->process_name);
      k_write_log(message);
    }
  }
  return 0;
};

int k_change_priority(pid_t pid, int priority) {
  // invalid if PID doesn't exist
  pcb* proc = PCBDequeJobSearch(PCBList, pid);
  if (proc == NULL) {
    return -1;
  }
  if (P_WIFEXITED(proc->status) || P_WIFSIGNALED(proc->status)) {
    // invalid if status is TERMINATED or FINISHED
    return -1;
  }

  // in running state, it is in a priority list, move to next priority list
  if (P_WIFRUNNING(proc->status)) {
    PIDSearchAndDelete(priorityList[proc->priority], pid);
    PIDDeque_Push_Back(priorityList[priority], pid);
  }

  char message[100];
  sprintf(message, "[%3d]\tNICE     \t%d\t%d\t%d\t%-15s\n", ticks, proc->pid,
          proc->priority, priority, proc->process_name);
  k_write_log(message);

  // change priority level in PCB
  proc->priority = priority;
  return 0;
}

void k_exit() {  // if parent is killed, automatically kill children??
  pcb* proc = PCBDequeJobSearch(PCBList, currentJob);
  // job is in running state, move to inactive
  if (P_WIFRUNNING(proc->status)) {
    PIDSearchAndDelete(priorityList[proc->priority], proc->pid);
    PIDDeque_Push_Back(priorityList[3], proc->pid);
  }
  // set status to finished
  proc->status = STATUS_FINISHED;

  // inform parent via an update to status_changed
  pcb* parent = PCBDequeJobSearch(PCBList, proc->parent_pid);
  if (parent != NULL) {
    PIDDeque_Push_Back(parent->status_changes, proc->pid);
  }

  char message[100];
  sprintf(message, "[%3d]\tEXITED   \t%d\t%d\t%-15s\n", ticks, proc->pid,
          proc->priority, proc->process_name);
  k_write_log(message);

  if (proc->parent_pid != -1) {
    char message[100];
    sprintf(message, "[%3d]\tZOMBIE   \t%d\t%d\t%-15s\n", ticks, proc->pid,
            proc->priority, proc->process_name);
    k_write_log(message);
  }

  PIDDeque* children = proc->child_pids;
  PIDDqNode* head = children->front;
  while (head != NULL) {
    pcb* child_proc = PCBDequeJobSearch(PCBList, head->pid);
    if (child_proc == NULL) {
      head = head->next;
      continue;
    }
    char message[100];
    sprintf(message, "[%3d]\tORPHAN   \t%d\t%d\t%-15s\n", ticks,
            child_proc->pid, child_proc->priority, child_proc->process_name);
    k_write_log(message);
    break;
  }

  // if curr process is blocking the parent, unstop the parent
  // move parent back to running state
  if (proc->blocking) {
    parent->status = STATUS_RUNNING;
    if (PIDSearchAndDelete(priorityList[3], parent->pid)) {
      PIDDeque_Push_Back(priorityList[parent->priority], parent->pid);
    }
    char message[100];
    sprintf(message, "[%3d]\tUNBLOCKED\t%d\t%d\t%-15s\n", ticks, parent->pid,
            parent->priority, parent->process_name);
    k_write_log(message);
  }
}

pid_t k_waitpid(pid_t pid, int* wstatus, bool nohang) {
  // Special case: wait on all children
  if (pid == -1) {
    pcb* parent = PCBDequeJobSearch(PCBList, currentJob);
    PIDDeque* children = parent->child_pids;

    if (PIDDeque_Size(children) == 0) {
      return -1;
    }

    if (!nohang) {
      PIDDqNode* head = children->front;
      while (head != NULL) {
        pcb* child_proc = PCBDequeJobSearch(PCBList, head->pid);
        child_proc->blocking = 1;
        head = head->next;
      }
    }

    PIDDqNode* head = children->front;
    while (head != NULL) {
      pcb* child_proc = PCBDequeJobSearch(PCBList, head->pid);
      if (child_proc == NULL) {
        head = head->next;
        continue;
      }
      if (child_proc->status == STATUS_FINISHED ||
          child_proc->status == STATUS_TERMINATED) {
        if (wstatus != NULL) {
          *wstatus = child_proc->status;
        }
        pid_t to_return = child_proc->pid;
        char message[100];
        sprintf(message, "[%3d]\tWAITED   \t%d\t%d\t%-15s\n", ticks,
                child_proc->pid, child_proc->priority,
                child_proc->process_name);
        k_write_log(message);
        k_handle_status_changes(child_proc->pid);

        return to_return;
      }
      head = head->next;
    }

    if (!nohang) {
      // set parent status to waiting
      parent->status = STATUS_BLOCKED;

      // move to inactive jobs queue and wait
      PIDSearchAndDelete(priorityList[parent->priority], parent->pid);
      if (!PIDDequeJobSearch(priorityList[3], parent->pid)) {
        PIDDeque_Push_Back(priorityList[3], parent->pid);
      }

      PIDDqNode* head = children->front;
      while (head != NULL) {
        pcb* child_proc = PCBDequeJobSearch(PCBList, head->pid);
        if (child_proc == NULL) {
          head = head->next;
          continue;
        }
        child_proc->blocking = 1;
        head = head->next;
      }

      char message[100];
      sprintf(message, "[%3d]\tBLOCKED  \t%d\t%d\t%-15s\n", ticks, parent->pid,
              parent->priority, parent->process_name);
      k_write_log(message);

      spthread_suspend(parent->curr_thread);
    }

    head = children->front;
    while (head != NULL) {
      pcb* child_proc = PCBDequeJobSearch(PCBList, head->pid);
      if (child_proc == NULL) {
        continue;
      }
      if (child_proc->status == STATUS_FINISHED ||
          child_proc->status == STATUS_TERMINATED) {
        if (wstatus != NULL) {
          *wstatus = child_proc->status;
        }

        pid_t to_return = child_proc->pid;
        char message[100];
        sprintf(message, "[%3d]\tWAITED   \t%d\t%d\t%-15s\n", ticks,
                child_proc->pid, child_proc->priority,
                child_proc->process_name);
        k_write_log(message);
        k_handle_status_changes(child_proc->pid);
        return to_return;
      }
      head = head->next;
    }
    return 0;
  }

  // Specified pid
  pcb* child_proc = PCBDequeJobSearch(PCBList, pid);

  // error case 1: pid doesn't exist
  if (child_proc == NULL) {
    return -1;
  }

  pcb* parent = PCBDequeJobSearch(PCBList, currentJob);
  // error case 2: pid is not a child process of the calling parent
  if (!PIDDequeJobSearch(parent->child_pids, pid)) {
    return -1;
  }

  if (child_proc->status == STATUS_FINISHED ||
      child_proc->status == STATUS_TERMINATED) {
    if (wstatus != NULL) {
      *wstatus = child_proc->status;
    }

    pid_t to_return = child_proc->pid;
    char message[100];
    sprintf(message, "[%3d]\tWAITED   \t%d\t%d\t%-15s\n", ticks,
            child_proc->pid, child_proc->priority, child_proc->process_name);
    k_write_log(message);
    k_handle_status_changes(child_proc->pid);
    return to_return;
  }

  if (!nohang) {
    // set parent status to waiting
    parent->status = STATUS_BLOCKED;
    PIDSearchAndDelete(priorityList[parent->priority], parent->pid);
    if (!PIDDequeJobSearch(priorityList[3], parent->pid)) {
      PIDDeque_Push_Back(priorityList[3], parent->pid);
    }
    // the child sets blocking to true
    child_proc->blocking = 1;

    char message[100];
    sprintf(message, "[%3d]\tBLOCKED  \t%d\t%d\t%-15s\n", ticks, parent->pid,
            parent->priority, parent->process_name);
    k_write_log(message);

    spthread_suspend(parent->curr_thread);
  }
  if (wstatus != NULL) {
    *wstatus = child_proc->status;
  }
  char message[100];
  sprintf(message, "[%3d]\tWAITED   \t%d\t%d\t%-15s\n", ticks, child_proc->pid,
          child_proc->priority, child_proc->process_name);
  k_write_log(message);
  k_handle_status_changes(child_proc->pid);
  return pid;
}

void k_sleep(unsigned int seconds) {
  pcb* proc = PCBDequeJobSearch(PCBList, currentJob);
  if (proc->status == STATUS_FINISHED || proc->status == STATUS_TERMINATED) {
    return;
  }
  // current process running MUST be a job which is SIGRUNNING...
  proc->status = STATUS_BLOCKED;
  proc->sleep_duration = seconds * 10;
  // move to inactive jobs list
  PIDSearchAndDelete(priorityList[proc->priority], proc->pid);
  PIDDeque_Push_Back(priorityList[3], proc->pid);

  char message[100];
  sprintf(message, "[%3d]\tBLOCKED  \t%d\t%d\t%-15s\n", ticks, proc->pid,
          proc->priority, proc->process_name);
  k_write_log(message);
  return;
}

void k_proc_cleanup(pcb* proc) {
  if (proc == NULL) {
    return;
  }
  if (proc->is_background && (proc->status == STATUS_FINISHED) &&
      proc->parent_pid == 1) {
    char message[1024];
    char plus = (proc->pid == plus_pid) ? '+' : ' ';
    sprintf(message, "[%d]%c done %s\n", proc->job_id, plus,
            command_print_helper(proc->parsed->commands));
    k_write(STDOUT_FILENO, message, strlen(message));
  }

  PIDDeque* children = proc->child_pids;
  // remove from the priority list
  PIDSearchAndDelete(priorityList[proc->priority], proc->pid);
  PIDSearchAndDelete(priorityList[3], proc->pid);
  pid_t curr_child = -1;
  // recursively clean up children a well
  while (PIDDeque_Peek_Front(children, &curr_child)) {
    pcb* child_proc = PCBDequeJobSearch(PCBList, curr_child);
    k_proc_cleanup(child_proc);
    PIDDeque_Pop_Front(children);
  }

  // Have to remove from parent's child list
  if (proc->parent_pid != -1) {
    pcb* parent = PCBDequeJobSearch(PCBList, proc->parent_pid);
    PIDDeque* parent_children = parent->child_pids;
    PIDSearchAndDelete(parent_children, proc->pid);

    // Move parent back into active queue if it was blocking
    if (proc->blocking &&
        PIDDequeJobSearch(priorityList[3], proc->parent_pid)) {
      PIDSearchAndDelete(priorityList[3], proc->parent_pid);
      PIDDeque_Push_Back(priorityList[parent->priority], parent->pid);
    }
  }

  PCBSearchAndDelete(PCBList, proc->pid, true);
  return;
}

void k_sleep_check() {
  // iterate the inactive job queue
  PIDDeque* inactives = priorityList[3];
  pid_t pid = -1;
  // recursively clean up children a well
  PIDDqNode* curr = inactives->front;
  while (curr != NULL) {
    pid = curr->pid;
    pcb* proc = PCBDequeJobSearch(PCBList, pid);
    if (proc->sleep_duration > 0 &&
        proc->status == STATUS_BLOCKED) {  // it is a sleeping job
      proc->sleep_duration--;              // decrement sleep count by 1
      if (proc->sleep_duration == 0) {  // if sleep count reaches 0, unsleep and
                                        // change status to finished
        proc->status = STATUS_FINISHED;  // set status to finished
        // inform parent via an update to status_changed
        pcb* parent = PCBDequeJobSearch(PCBList, proc->parent_pid);
        if (parent != NULL) {
          PIDDeque_Push_Back(parent->status_changes, pid);
        }
        // if curr process is blocking the parent, unstop the parent
        // move parent back to running state
        if (proc->blocking) {
          parent->status = STATUS_RUNNING;
          PIDSearchAndDelete(priorityList[3], parent->pid);
          PIDDeque_Push_Back(priorityList[parent->priority], parent->pid);
        }
      }
    }
    curr = curr->next;
  }
}

void k_handle_status_changes(pid_t target_pid) {
  // for now, just handle finished jobs by calling k_proc_cleanup
  pcb* proc = PCBDequeJobSearch(PCBList, currentJob);
  PIDDeque* stat_changes = proc->status_changes;
  pid_t job = -1;

  PIDDqNode* curr = stat_changes->front;
  while (curr != NULL) {
    job = curr->pid;
    pcb* changed_job = PCBDequeJobSearch(PCBList, job);
    if (changed_job == NULL) {
      curr = curr->next;
      continue;
    }
    curr = curr->next;
    if (changed_job->pid == target_pid &&
        (changed_job->status == STATUS_FINISHED ||
         changed_job->status == STATUS_TERMINATED)) {
      k_proc_cleanup(changed_job);
      PIDSearchAndDelete(stat_changes, job);
    }
  }
  return;
}

void k_write_log(char* message) {
  write(logfd, message, strlen(message));
}

/**
 * Example execution:
 $ ps
  PID PPID PRI STAT CMD
    1    0   0  B   shell
    2    1   1  S   sleep
    3    1   1  R   ps
*/
void k_ps() {
  // write header to stdout
  char* header = "PID\tPPID\tPRI\tSTAT\tCMD\n";

  pcb* curr_job = PCBDequeJobSearch(PCBList, currentJob);

  k_write(curr_job->process_fdt[1], header, strlen(header) + 1);

  PCBDqNode* curr_node = PCBList->front;
  while (curr_node != NULL) {
    pcb* proc = curr_node->pcb;
    char message[100];
    // fill message up with each process id info, inshallah it does not overflow
    sprintf(message, "%d\t%d\t%d\t%s\t%s\n", proc->pid, proc->parent_pid,
            proc->priority, get_status(proc->status), proc->process_name);
    k_write(curr_job->process_fdt[1], message, strlen(message) + 1);
    curr_node = curr_node->next;
  }
}

int k_handle_bg(pid_t pid) {
  // curr_job is pcd of either most recently stopped job or backgrounded job
  pcb* curr_job = NULL;
  if (pid == -1) {
    curr_job = PCBDequeStopSearch(PCBList);
  } else {
    curr_job = PCBDequeJobSearch(PCBList, pid);
  }
  // job doesn't exist, or job is not stopped,
  if (curr_job == NULL || curr_job->status != STATUS_STOPPED ||
      curr_job->stop_time == -1) {
    return -1;
  }
  bool is_sleep = strcmp(curr_job->process_name, "sleep") == 0;
  curr_job->status = is_sleep ? STATUS_BLOCKED : STATUS_RUNNING;
  curr_job->stop_time = 0;
  // remove from inactive queue if inactive
  if (!is_sleep) {
    PIDSearchAndDelete(priorityList[3], curr_job->pid);
    // add to priority list if it's not there
    if (!PIDDequeJobSearch(priorityList[curr_job->priority], curr_job->pid)) {
      PIDDeque_Push_Back(priorityList[curr_job->priority], curr_job->pid);
    }
  }
  // inform the parent
  char message[1024];
  // fill message up with each process id info, inshallah it does not overflow
  sprintf(message, "[%d] %d continued %s\n", curr_job->job_id, curr_job->pid,
          command_print_helper((curr_job->parsed)->commands));
  k_write(STDOUT_FILENO, message, strlen(message) + 1);
  return 0;
}

int k_handle_fg(pid_t pid) {
  // curr_job is pcd of either most recently stopped job or backgrounded job
  pcb* curr_job = NULL;
  if (pid == -1) {
    curr_job = PCBDequeStopSearch(PCBList);
    if (curr_job == NULL) {
      curr_job = PCBDequeBackgroundSearch(PCBList);
    }
  } else {
    curr_job = PCBDequeJobSearch(PCBList, pid);
  }
  if (curr_job == NULL || curr_job->status == STATUS_FINISHED ||
      curr_job->status == STATUS_TERMINATED) {
    return -1;
  }
  bool is_sleep = strcmp(curr_job->process_name, "sleep") == 0;

  curr_job->is_background = false;
  curr_job->status = is_sleep ? STATUS_BLOCKED : STATUS_RUNNING;
  curr_job->stop_time = 0;
  curr_job->blocking = true;
  fgJob = curr_job->pid;

  pcb* parent = PCBDequeJobSearch(PCBList, curr_job->parent_pid);

  // remove from inactive queue if inactive
  if (!is_sleep) {
    PIDSearchAndDelete(priorityList[3], curr_job->pid);
    // add to priority list if it's not there
    if (!PIDDequeJobSearch(priorityList[curr_job->priority], curr_job->pid)) {
      PIDDeque_Push_Back(priorityList[curr_job->priority], curr_job->pid);
    }
  }
  parent->status = STATUS_BLOCKED;
  PIDSearchAndDelete(priorityList[parent->priority], parent->pid);
  PIDDeque_Push_Back(priorityList[3], parent->pid);

  char message[1024];
  sprintf(message, "[%d] %d running %s\n", curr_job->job_id, curr_job->pid,
          command_print_helper((curr_job->parsed)->commands));
  k_write(STDOUT_FILENO, message, strlen(message) + 1);
  return 0;
}

char* get_status(int status) {
  if (status == STATUS_RUNNING) {
    return "R";
  } else if (status == STATUS_STOPPED) {
    return "S";
  } else if (status == STATUS_BLOCKED) {
    return "B";
  } else if (status == STATUS_FINISHED) {
    return "Z";
  } else if (status == STATUS_TERMINATED) {
    return "Z";
  }
  return "";
}