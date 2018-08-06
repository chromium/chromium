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
#ifndef SPLIT_H
#define SPLIT_H

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

/// @brief Performs split operation on input.
/// @details splits the input data into n parts, for each user provides name and offsets.
/// @n User cannot use split primitive directly.
/// @n It is needed to refer to the output ids with the name "<split_prim_id>:<split_output_id>".
/// @n
/// @n\b Assumptions 
/// @n - offsets1 < offsets2 < offsets3 < ...
/// @n - size[n] = offsets[n+1] - offsets[n];
/// @n - last element: size[n] = split_input.size - offsets[n];
/// @n - no buffer overlapping, as the output size is calculated using offset and input size
/// @n - split primitive id cannot be used by any other primitive (user needs to use output_ids only)
/// @n Breaking any of this conditions will cause exeption throw.
/// @n
/// @n\b Example:
/// @n Splitting output to 2 parts by the features:
/// @n input_size = { 2, 4, 3, 5 };
/// @n split_id = "split";
/// @n output_ids_offsets[0] = { "out0", { 0,0,0,0 } };
/// @n output_ids_offsets[1] = { "out1", { 0,2,0,0 } };
/// @n After split there would be 2 primitives: "split:out0" and "split:out1" which contain 2 feature maps (lower and upper)

CLDNN_BEGIN_PRIMITIVE_DESC(split)
/// @brief List of output_ids.
cldnn_primitive_id_arr output_ids;
/// @brief Array of tensors with offsets.
cldnn_tensor_arr output_offsets;
CLDNN_END_PRIMITIVE_DESC(split)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(split);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* SPLIT_H */

