#pragma once

#include <stdint.h>

void memory_init(void *base, uint16_t len);
void *get_mem(int len);
void free_mem(void *addr);
