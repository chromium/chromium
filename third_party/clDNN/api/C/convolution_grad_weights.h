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
#ifndef CONVOLUTION_GRAD_WEIGHTS_H
#define CONVOLUTION_GRAD_WEIGHTS_H

#include <stdbool.h>
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

/// @brief Performs backward convolution operation for weights and biases.
/// @details convolution_grad_weights updates weights and bias mutable data for training purposes.
/// @details Please note that this primitive was not heavily tested and currently only batch=1 is enabled for this primitive.
CLDNN_BEGIN_PRIMITIVE_DESC(convolution_grad_weights)
/// @brief Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_weights window should start calculations.
cldnn_tensor input_offset;
/// @brief Defines the spatial dimensions of stride of adjacent elements in input buffer.
cldnn_tensor stride;
/// @brief Defines gaps in the input - dilation rate k=1 is normal convolution, k=2 means skipping one pixel per input, k=4 means skipping 3 pixels.
/// As an example in one dimension, a filter w of size 3 would compute over input x the following: w[0]*x[0] + w[1]*x[1] + w[2]*x[2] for dilation of 1. 
/// For dilation 2 the filter would instead compute w[0]*x[0] + w[1]*x[2] + w[2]*x[4].
cldnn_tensor dilation;
/// @brief On how many cards split the computation to.
uint32_t split;
/// @brief Array of primitive ids containing weights data. Size of array should be equivalent to @p split.
cldnn_primitive_id_arr weights;
/// @brief Array of primitive ids containing bias data. Size of array should be equivalent to @p split or should be empty (if not using bias).
cldnn_primitive_id_arr bias;
/// @brief Primitive id containing convolution gradient data. Used for proper order of gradient calculation. Leave empty if primitive is last in backward pass.
cldnn_primitive_id conv_grad;
/// @brief Array of primitive ids containing weights gradient data calculated in previous iteration. Amount of primitives and their memory sizes should be same as weights.
cldnn_primitive_id_arr prev_weights_grad;
/// @brief Array of primitive ids containing bias gradient data calculated in previous iteration. Amount of primitives and their memory sizes should be same as biases.
cldnn_primitive_id_arr prev_bias_grad;
/// @brief Should primitive give weights gradient (delta) as an output
bool output_grad_w;

CLDNN_END_PRIMITIVE_DESC(convolution_grad_weights)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(convolution_grad_weights);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* CONVOLUTION_GRAD_WEIGHTS_H */

