#pragma once
#include "list.h"

struct tick_work_q {
    void (*tick_fn)(void);
    list tick_work_l;
};

void tick_add_work_fn(struct tick_work_q *new_work);
