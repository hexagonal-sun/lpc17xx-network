#pragma once
#include <stdint.h>

#include "string.h"

typedef struct
{
    uint8_t *buffer;
    uint8_t *head;
    uint8_t *tail;
    size_t buf_sz;
    size_t free_space;
}circular_buf;

void circular_buf_init(circular_buf *cbuf, size_t capacity);
void circular_buf_free(circular_buf *cbuf);
size_t circular_buf_cur_capacity(circular_buf *cbuf);
size_t circular_buf_cur_usage(circular_buf *cbuf);
void circular_buf_push(circular_buf *cbuf, uint8_t *data, size_t len);
void circular_buf_pop(circular_buf *cbuf, uint8_t *dst_buf, size_t len);
