#ifndef FAT_GLOBALS_H
#define FAT_GLOBALS_H

#include "./fat_helper.h"

typedef struct global_fdt {
  char name[32];
  uint8_t perm;
  uint16_t firstBlock;
  uint32_t offset;
  uint32_t size;
  bool open;
} global_fdt;

extern int fs_fd;               // File Descriptor for FAT
extern uint16_t* fat;           // FAT
extern global_fdt g_fdt[1024];  // Global File Descriptor Table
extern int g_counter;           // Counter for global FDT

#endif