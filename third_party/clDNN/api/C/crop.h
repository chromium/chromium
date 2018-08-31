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
#ifndef CROP_H
#define CROP_H

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

/// @brief Performs crop operation on input.
/// @details Crops the input to the shape of reference_input accross all dimensions taking into account specified input offsets.
/// @n
/// @n\b Examples
/// @n Crop without offset example:
/// \image html crop_no_offset.jpg
/// @n Crop with offset example:
/// \image html crop_w_offset.jpg
/// @n
/// @n\b Requirements 
/// @n - Input, reference and offset layout (order) has to be the same
/// @n - Input size cannot be greater than reference size in any dimension
/// @n - All sizes have to have positive numbers
/// @n - Reference size plus offset cannot exceed input size
/// @n Breaking any of this conditions will cause exeption throw.

CLDNN_BEGIN_PRIMITIVE_DESC(crop)
/// @brief Reference input tensor with the required dimensions.
cldnn_tensor reference_input;
/// @brief Input offsets.
cldnn_tensor offsets;
CLDNN_END_PRIMITIVE_DESC(crop)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(crop);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* CROP_H */

