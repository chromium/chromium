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
#ifndef CONVOLUTION_GRAD_INPUT_H
#define CONVOLUTION_GRAD_INPUT_H

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

/// @brief Performs transposed convolution.
/// @details convolution_grad_input is similar to convolution layer with the weights flipped on the axis and stride and input padding parameters used in opposite sense as in convolution.
CLDNN_BEGIN_PRIMITIVE_DESC(convolution_grad_input)
/// @brief Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_input window should start calculations.
cldnn_tensor input_offset;
/// @brief Defines the spatial dimensions of stride of adjacent elements in input buffer.
cldnn_tensor stride;
/// @brief On how many cards split the computation to.
uint32_t split;
/// @brief Indicates that the primitive has user-defined output size (non-zero value).
uint32_t with_output_size;
/// @brief User-defined output data size of the primitive (w/o padding).
cldnn_tensor output_size;
/// @brief Array of primitive ids containing weights data. Size of array should be equivalent to @p split.
cldnn_primitive_id_arr weights;
CLDNN_END_PRIMITIVE_DESC(convolution_grad_input)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(convolution_grad_input);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* CONVOLUTION_GRAD_INPUT_H */

