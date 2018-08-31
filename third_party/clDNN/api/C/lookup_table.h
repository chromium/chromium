/*
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
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

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

/// @brief Enum type to specify axis to return values from.
typedef enum
{
    cldnn_lookup_table_batch,
    cldnn_lookup_table_feature,
    cldnn_lookup_table_x,
    cldnn_lookup_table_y,
    cldnn_lookup_table_xyf
} cldnn_lookup_table_axis;

/// @brief Returns values from data on which given indices are pointing at.
CLDNN_BEGIN_PRIMITIVE_DESC(lookup_table)
/// @brief Axis to return values from. If not set, returns data which index is pointing at in the flattened x, y, f dimensions for each batch.
cldnn_lookup_table_axis axis;
/// @brief Indicates that the primitive has user defined axis to return values from.
uint32_t with_axis;
CLDNN_END_PRIMITIVE_DESC(lookup_table)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(lookup_table);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* LOOKUP_TABLE.H */

