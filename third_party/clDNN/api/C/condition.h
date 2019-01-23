// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

///////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CONDITION_H
#define CONDITION_H

#include "cldnn.h"


/// @addtogroup c_api C API
/// @{
/// @addtogroup c_topology Network Topology
/// @{
/// @addtogroup c_primitives Primitives
/// @{

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Function, which will be used during comparison.
typedef enum /*:int32_t*/
{
    EQUAL,
    GREATER,
    LESS
} cldnn_cond_functions;

/// @brief Adds primitive, which works like "if".
///
/// @details
/// @n   Applies comparision between 2 inputs.
/// @n   Compare data - sizes of that input specifes the range of the comparison.
/// @n   Offset - offset in memory, when comparing values.
CLDNN_BEGIN_PRIMITIVE_DESC(condition)
/// @brief An identifier of topology, which will be executed when comparison returns true.
cldnn_topology topology_true;
/// @brief An identifier of topology, which will be executed when comparison returns false.
cldnn_topology topology_false;
/// @brief An identifier of primitive which contains compare values.
cldnn_primitive_id compare_data;
/// @brief Used function during comparison.
cldnn_cond_functions function;
/// @brief Offset for compare data.
cldnn_tensor offset;

CLDNN_END_PRIMITIVE_DESC(condition)
CLDNN_DECLARE_PRIMITIVE_TYPE_ID(condition);


#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif // CONDITION_H
