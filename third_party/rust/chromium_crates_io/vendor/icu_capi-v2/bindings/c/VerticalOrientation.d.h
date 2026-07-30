#ifndef VerticalOrientation_D_H
#define VerticalOrientation_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum VerticalOrientation {
  VerticalOrientation_Rotated = 0,
  VerticalOrientation_TransformedRotated = 1,
  VerticalOrientation_TransformedUpright = 2,
  VerticalOrientation_Upright = 3,
} VerticalOrientation;

typedef struct VerticalOrientation_option {union { VerticalOrientation ok; }; bool is_ok; } VerticalOrientation_option;



#endif // VerticalOrientation_D_H
