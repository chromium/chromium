#ifndef GraphemeClusterBreak_H
#define GraphemeClusterBreak_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "GraphemeClusterBreak.d.h"






uint8_t icu4x_GraphemeClusterBreak_to_integer_mv1(GraphemeClusterBreak self);

typedef struct icu4x_GraphemeClusterBreak_from_integer_mv1_result {union {GraphemeClusterBreak ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_from_integer_mv1_result;
icu4x_GraphemeClusterBreak_from_integer_mv1_result icu4x_GraphemeClusterBreak_from_integer_mv1(uint8_t other);






#endif // GraphemeClusterBreak_H
