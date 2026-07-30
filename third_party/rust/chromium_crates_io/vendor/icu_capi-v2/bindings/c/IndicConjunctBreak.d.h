#ifndef IndicConjunctBreak_D_H
#define IndicConjunctBreak_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum IndicConjunctBreak {
  IndicConjunctBreak_None = 0,
  IndicConjunctBreak_Consonant = 1,
  IndicConjunctBreak_Extend = 2,
  IndicConjunctBreak_Linker = 3,
} IndicConjunctBreak;

typedef struct IndicConjunctBreak_option {union { IndicConjunctBreak ok; }; bool is_ok; } IndicConjunctBreak_option;



#endif // IndicConjunctBreak_D_H
