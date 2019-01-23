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
#ifndef BROADCAST_H
#define BROADCAST_H

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

/// @brief Broadcasts input to specified output size (broadcast size).
///
/// @details Takes input and copies it to output once or multiple times, until output will
///          reach the sizes specified in @p broadcast_sizes.
/// @n
/// @n Lets assume that:
/// @n      <tt>input_sizes = (in_b, in_f, in_y, in_x)</tt>
/// @n      <tt>broadcast_sizes = (bs_b, bs_f, bs_y, bs_x)</tt>
/// @n The input is broadcasted on each dimension where <tt>bs_{dim} > in_{dim}</tt> and <tt>bs_{dim}</tt>
///    is dividable by <tt>in_{dim}</tt> (input is copied <tt>bs_{dim} / in_{dim}</tt> times).
///    The dimensions where <tt>bs_{dim}</tt> is equal to <tt>in_{dim}</tt> remain unchanged.
/// @n The resulting output will have sizes equal to @p broadcast_sizes and contains values from
///    input that meet following criteria:
/// @n      <tt>output[(b, f, y, x)] = input[(b % in_b, f % in_f, y % in_y, x % in_x)]</tt>
/// @n where <tt>(b, f, y, x)</tt> is a position of value in a primitive output.
/// @n
/// @n@b Requirements:
/// @n - @p broadcast_sizes must be positive on all dimensions and compatible
///      with size of input (describe the same dimensions).
/// @n - @p broadcast_sizes must be greater than or equal to input sizes on
///      all dimensions. (For any dimension, if @p broadcast_sizes is lower
///      than input size on the dimension then @p broadcast_sizes will be replaced
///      by input size on this dimension.)
/// @n - For any dimension, if @p broadcast_sizes is greater than input size on
///      the dimension then @p broadcast_sizes must be dividable by input size
///      on this dimension.
/// @n Breaking any of these conditions will raise an exeption.
CLDNN_BEGIN_PRIMITIVE_DESC(broadcast)
/// @brief Sizes of broadcast. Output size of current primitive will match broadcast sizes (layout type
///        will not change).
///        If @p broadcast_sizes are not specified (all zeros), the input sizes are used as @p broadcast_sizes.
cldnn_tensor broadcast_sizes;
CLDNN_END_PRIMITIVE_DESC(broadcast)


CLDNN_DECLARE_PRIMITIVE_TYPE_ID(broadcast);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif // BROADCAST_H
