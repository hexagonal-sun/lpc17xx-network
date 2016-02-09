#pragma once
#include <stdint.h>

typedef uint8_t mutex_t;

void get_lock(mutex_t *lock);
int  try_lock(mutex_t *lock);
void release_lock(mutex_t *lock);
