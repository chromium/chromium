#ifndef DecimalRoundingIncrement_D_H
#define DecimalRoundingIncrement_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DecimalRoundingIncrement {
  DecimalRoundingIncrement_MultiplesOf1 = 0,
  DecimalRoundingIncrement_MultiplesOf2 = 1,
  DecimalRoundingIncrement_MultiplesOf5 = 2,
  DecimalRoundingIncrement_MultiplesOf25 = 3,
} DecimalRoundingIncrement;

typedef struct DecimalRoundingIncrement_option {union { DecimalRoundingIncrement ok; }; bool is_ok; } DecimalRoundingIncrement_option;



#endif // DecimalRoundingIncrement_D_H
