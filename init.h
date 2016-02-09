#pragma once

typedef void (*initcall_t)(void);

#define __define_initcall(fn, lvl)                      \
    static initcall_t __initcall_##fn##lvl              \
    __attribute__((__section__(".initcall_" #lvl))) = fn;

#define early_initcall(fn) __define_initcall(fn, early)
#define initcall(fn) __define_initcall(fn, 0)
