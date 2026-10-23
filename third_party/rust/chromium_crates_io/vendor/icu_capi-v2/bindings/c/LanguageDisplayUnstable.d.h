#ifndef LanguageDisplayUnstable_D_H
#define LanguageDisplayUnstable_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum LanguageDisplayUnstable {
  LanguageDisplayUnstable_Dialect = 0,
  LanguageDisplayUnstable_Standard = 1,
} LanguageDisplayUnstable;

typedef struct LanguageDisplayUnstable_option {union { LanguageDisplayUnstable ok; }; bool is_ok; } LanguageDisplayUnstable_option;



#endif // LanguageDisplayUnstable_D_H
