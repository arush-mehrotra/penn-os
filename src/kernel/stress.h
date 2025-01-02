#ifndef STRESS_H
#define STRESS_H

/**
 * @brief Spawn 10 children and hanging wait on all of them
 *
 */
void* hang(void*);

/**
 * @brief Spawn 10 children but don't hanging wait on them
 *
 */
void* nohang(void*);

/**
 * @brief Spawn 26 children recursively
 *
 */
void* recur(void*);

#endif
