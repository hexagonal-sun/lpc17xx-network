#pragma once

#define wait_for_volatile_condition(condition)  \
    ({                                          \
      for (;;) {                                \
      asm volatile("" : : : "memory");          \
      if (condition)                            \
          break;                                \
      asm volatile("wfe");                      \
      }                                         \
    })
