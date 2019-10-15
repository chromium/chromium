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

/// @brief Broadcasts input to defined by @p broadcast_sizes output. @p broadcast_axes are used to
///        reinterpret input (reshape) inside algorithm.
///
/// @details Takes input, reinterpret it according to @p broadcast_axes
///          and copies it to output once or multiple times.
/// @n
/// @n Simple example with empty @p broadcast_axes. Lets assume that:
/// @n      <tt>input_sizes = (in_b, in_f, in_y, in_x)</tt>
/// @n      <tt>broadcast_sizes = (bs_b, bs_f, bs_y, bs_x)</tt>
/// @n      <tt>broadcast_axes = () - empty</tt>
/// @n The input is broadcasted on each dimension where <tt>bs_{dim} > in_{dim}</tt> and <tt>bs_{dim}</tt>
///    is dividable by <tt>in_{dim}</tt> (input is copied <tt>bs_{dim} / in_{dim}</tt> times).
///    The dimensions where <tt>bs_{dim}</tt> is equal to <tt>in_{dim}</tt> remain unchanged.
/// @n The resulting output will have sizes equal to @p broadcast_sizes and contains values from
///    input that meet following criteria:
/// @n      <tt>output[(b, f, y, x)] = input[(b % in_b, f % in_f, y % in_y, x % in_x)]</tt>
/// @n where <tt>(b, f, y, x)</tt> is a position of value in a primitive output.
/// @n
/// @n More complicated example with non empty @p broadcast_axes. Lets assume that:
/// @n      <tt>broadcast_sizes = (bs_b, bs_f, bs_y, bs_x)</tt>
/// @n      <tt>broadcast_axes = (2)</tt>
/// @n Taking into account broadcast_axes size (=1) primitive's input must be (4 - 1 = 3):
/// @n      <tt>primitive input = (1, in_b, in_f, in_x)</tt>
/// @n Due to broadcast_axes = (2) primitive will interpret input as:
/// @n      <tt>primitive input(internal representation) = (in_b, in_f, 1, in_x)</tt>
/// @n Now, you can apply broadcast rules from previous example to modified (reinterpreted)
///    input and output:
/// @n      <tt>input_sizes = (in_b, in_f, 1, in_x)</tt>
/// @n      <tt>output_shape = (bs_b, bs_f, bs_y, bs_x)</tt>
/// @n      <tt>broadcast_axes = () - empty</tt>
/// @n
/// @n@b Requirements:
/// @n - @p broadcast_sizes must be positive on all dimensions.
/// @n - @p broadcast_axes size (dimensions count) must be within (inclusive) range
///      0 - 4.
/// @n - @p broadcast_axes mustn't have duplicate values.
/// @n - Values of @p broadcast_axes must be within (inclusive) range 0 - 3
/// @n - @p output_shape must be greater (dividable) than or equal to reinterpreted
///      input on all dimensions.
/// @n Breaking any of these conditions will raise an exception.
CLDNN_BEGIN_PRIMITIVE_DESC(broadcast)
/// @brief Sizes of broadcast. Output size of current primitive will match broadcast sizes (layout type
///        will not change).
cldnn_tensor broadcast_sizes;
/// @brief Array of axes positions from output shape (0-based, from left to right)
///        along which broadcast should happen.
cldnn_uint16_t_arr broadcast_axes;

CLDNN_END_PRIMITIVE_DESC(broadcast)


CLDNN_DECLARE_PRIMITIVE_TYPE_ID(broadcast);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif // BROADCAST_H
