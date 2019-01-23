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
#ifndef BORDER_H
#define BORDER_H

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

/// @brief Type of border that will be added to the input by current layer / primitive
///        ( @CLDNN_PRIMITIVE_DESC{border} ​).
typedef enum /*:int32_t*/
{
    /// @brief All points in the border are set to constant value.
    cldnn_border_constant,
    cldnn_border_zero = cldnn_border_constant, /// keep bwd compatibilty 
    /// @brief Border is constructed as an mirror of image (edge is also mirrored).
    /// @details Size of border in any dimension cannot be larger than size of
    ///          input in the same dimension.
    cldnn_border_mirror,
    /// @brief Border is constructed as an mirror of image (edge is NOT mirrored).
    /// @details Size of border in any dimension cannot be larger than size of
    ///          input in the same dimension decreased by @c 1.
    cldnn_border_mirror_101,
    /// @brief Border is constructed as an replication of edge.
    /// @details Size of border in any dimension cannot be larger than size of
    ///          input in the same dimension.
    cldnn_border_edge
} cldnn_border_type;


/// @brief Adds border around input.
///
/// @details Applies border of specified type around input data. The size of output data is increased
///          by @c left_top_sizes and by @right_bottom_sizes.
/// @n
/// @n@b Requirements:
/// @n - @c left_top_sizes and @c right_bottom_sizes must be non-negative on all dimensions and compatible
///      with size of input (describe the same dimensions).
/// @n - For @c border_type equal to @c cldnn_border_mirror, @c left_top_sizes and @c right_bottom_sizes
///      must be lower than or equal to size of input on corresponding dimension (for all dimensions)
/// @n - For @c border_type equal to @c cldnn_border_mirror_101, @c left_top_sizes and @c right_bottom_sizes
///      must be lower than size of input on corresponding dimension (for all dimensions)
CLDNN_BEGIN_PRIMITIVE_DESC(border)
/// @brief Size of border that needs to be added from left (in X dimension) and from top (in Y dimension).
cldnn_tensor left_top_sizes;
/// @brief Size of border that needs to be added from right (in X dimension) and from bottom (in Y dimension).
cldnn_tensor right_bottom_sizes;
/// @brief Type of border that needs to be added to the input.
cldnn_border_type border_type;
/// @brief Border value that is used in constant mode.
float border_value;
CLDNN_END_PRIMITIVE_DESC(border)


CLDNN_DECLARE_PRIMITIVE_TYPE_ID(border);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif // BORDER_H
