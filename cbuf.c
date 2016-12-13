#include "cbuf.h"
#include "memory.h"

void circular_buf_init(circular_buf *cbuf, size_t capacity)
{
    cbuf->buf_sz = cbuf->free_space = capacity;
    cbuf->tail = cbuf->head = cbuf->buffer = get_mem(capacity);
}

void circular_buf_free(circular_buf *cbuf)
{
    free_mem(cbuf->buffer);

    cbuf->buffer = cbuf->tail = cbuf->head = NULL;
    cbuf->buf_sz = 0;
}

size_t circular_buf_cur_capacity(circular_buf *cbuf)
{
    return cbuf->free_space;
}

size_t circular_buf_cur_usage(circular_buf *cbuf)
{
    return cbuf->buf_sz - cbuf->free_space;
}

void circular_buf_push(circular_buf *cbuf, uint8_t *data, size_t len)
{
    int i;

    if (len > cbuf->free_space)
        return;

    /* Could be quicker.... */
    for (i = 0; i < len; i++) {
        *cbuf->head = data[i];

        cbuf->head++;

        if (cbuf->head == cbuf->buffer + cbuf->buf_sz)
            cbuf->head = cbuf->buffer;
    }

    cbuf->free_space -= len;
}


void circular_buf_pop(circular_buf *cbuf, uint8_t *dst_buf, size_t len)
{
    int i;

    if (len > (cbuf->buf_sz - cbuf->free_space))
        return;

    /* Could be quicker.... */
    for (i = 0; i < len; i++) {
        dst_buf[i] = *cbuf->tail;

        cbuf->tail++;

        if (cbuf->tail == cbuf->buffer + cbuf->buf_sz)
            cbuf->tail = cbuf->buffer;
    }

    cbuf->free_space += len;
}
