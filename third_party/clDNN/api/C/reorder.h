/*
// Copyright (c) 2016 Intel Corporation
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
#ifndef REORDER_H
#define REORDER_H

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

/// @brief Changes how data is ordered in memory. Value type is not changed & all information is preserved.
/// @details Corresponding values are bitwise equal before/after reorder.
/// Also merged with subtraction layer, which can subtract, multiply or divide values based on mean_mode value, while doing reordering.
/// NOTE THAT THIS WILL SUBTRACT THE SAME VALUES FROM EACH BATCH.
CLDNN_BEGIN_PRIMITIVE_DESC(reorder)
/// @brief Requested memory format.
cldnn_format_type output_format;
/// @brief Requested memory data type.
cldnn_data_type output_data_type;
/// @brief Primitive id to get mean subtract values. Ignored if subtract_per_featrue is set.
cldnn_primitive_id mean_subtract;
/// @brief Array of mean subtract values.
cldnn_float_arr subtract_per_feature;
/// @brief Mode of mean execution
cldnn_reorder_mean_mode mean_mode;
CLDNN_END_PRIMITIVE_DESC(reorder)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(reorder);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* REORDER_H */

