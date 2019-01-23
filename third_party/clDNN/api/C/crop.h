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
/// @details Crops the input to the shape of reference_input across all dimensions taking into account specified input offsets.
/// @n       Borders variant calculated output shape from input shape minus the specified borders.
/// @n
/// @n\b Examples
/// @n Crop without offset example:
/// \image html crop_no_offset.jpg
/// @n Crop with offset example:
/// \image html crop_w_offset.jpg
/// @n
/// @n\b Requirements (reference size variant)
/// @n - Input size cannot be greater than reference size in any dimension
/// @n - All sizes have to have positive numbers
/// @n - Reference size plus offset cannot exceed input size
/// @n
/// @n\b Requirements (borders variant)
/// @n - Borders support batch, feature and spatial dimensions (rest of dimensions ignored).
/// @n - Input size cannot be greater than reference size in any dimension
/// @n - All sizes specified in borders have to have non-negative values (positive or @c 0).
/// @n - Sum of sizes of opposite borders must be lower than input size (on all non-ignored dimensions).
/// @n
/// @n Breaking any of this conditions will cause exception throw.
CLDNN_BEGIN_PRIMITIVE_DESC(crop)
/// @brief Reference input tensor with the required dimensions (if positive) or
///        negated value of right/bottom/upper border size (if non-positive).
cldnn_tensor reference_input;
/// @brief Input offsets (reference_input is positive) or left/top/lower border
///        size (reference_input is negative).
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

