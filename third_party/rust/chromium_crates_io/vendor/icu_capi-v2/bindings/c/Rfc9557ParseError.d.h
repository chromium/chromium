#ifndef Rfc9557ParseError_D_H
#define Rfc9557ParseError_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum Rfc9557ParseError {
  Rfc9557ParseError_Unknown = 0,
  Rfc9557ParseError_InvalidSyntax = 1,
  Rfc9557ParseError_OutOfRange = 2,
  Rfc9557ParseError_MissingFields = 3,
  Rfc9557ParseError_UnknownCalendar = 4,
} Rfc9557ParseError;

typedef struct Rfc9557ParseError_option {union { Rfc9557ParseError ok; }; bool is_ok; } Rfc9557ParseError_option;



#endif // Rfc9557ParseError_D_H
