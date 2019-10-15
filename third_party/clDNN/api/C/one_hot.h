// Copyright (c) 2019 Intel Corporation
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
#ifndef ONE_HOT_H
#define ONE_HOT_H

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

    /// @brief Creates a one-hot encoding of the input.
    /// @details Creates a one-hot encoding of the input, putting the new one-hot axis in the position
    /// @n       specified by the @p one_hot_axis input, using the @p shape tensor as size reference.
    /// @n       The size of @p shape must be appropriate for adding a one-hot axis to input. For example,
    /// @n      <tt>input_sizes = (1, in_f, in_y, in_x)</tt> 
    /// @n expanded with 
    /// @n      <tt>one_hot_axis = 2</tt> 
    /// @n would insert the one-hot axis in the Y dimension, requiring
    /// @n      <tt>shape = (in_f, in_y, one-hot_limit, in_x)</tt> 
    /// @n The output values would then be determined by input as
    /// @n      <tt>output[f, y, i, x] = (input[0, f, y, x] == i) ? 1 : 0;</tt>
    /// @n Since determining whether the input is appropriate (that the one-hot axis
    /// @n has enough space to fully encode all inputs) requires scanning the whole
    /// @n input, the primitive doesn't check for that, instead producing all-zeros
    /// @n output axes for inputs below 0 and greater than the limit set by
    /// @n @p shape.
    /// @n
    /// @n\b Requirements
    /// @n - @p one_hot_axis must be within (inclusive) range 0 - 3.
    /// @n - @p shape must fit input sizes (see example above).
    /// @n - input batch size must be equal to 1.
    /// @n
    /// @n Breaking any of this conditions will cause exception throw.
    CLDNN_BEGIN_PRIMITIVE_DESC(one_hot)
    /// @brief Output size reference.
    cldnn_tensor shape;
    /// @brief One-hot axis position in output shape (0-based, from left to right).
    uint16_t one_hot_axis;
    CLDNN_END_PRIMITIVE_DESC(one_hot)

        CLDNN_DECLARE_PRIMITIVE_TYPE_ID(one_hot);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* ONE_HOT_H */

