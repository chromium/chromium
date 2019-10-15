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
#ifndef DECONVOLUTION_H
#define DECONVOLUTION_H

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
/// Also supports built-in Relu @CLDNN_PRIMITIVE_DESC{activation} available by setting it in arguments.
/// @details Deconvolution is similar to convolution layer with the weights flipped on the axis and stride and input padding parameters used in opposite sense as in convolution.
CLDNN_BEGIN_PRIMITIVE_DESC(deconvolution)
/// @brief Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the deconvolution window should start calculations.
cldnn_tensor input_offset;
/// @brief Defines the spatial dimensions of stride of adjacent elements in input buffer.
cldnn_tensor stride;
/// @brief Enables Relu activation.
uint32_t with_activation;
/// @brief Relu activation slope.
float activation_negative_slope;
/// @brief On how many cards split the computation to.
uint32_t split;
/// @brief Indicates that the primitive has user-defined output size (non-zero value).
uint32_t with_output_size;
/// @brief User-defined output data size of the primitive (w/o padding).
cldnn_tensor output_size;
/// @brief Array of primitive ids containing weights data. Size of array should be equivalent to @p split.
cldnn_primitive_id_arr weights;
/// @brief Array of primitive ids containing bias data. Size of array should be equivalent to @p split or should be empty (if not using bias).
cldnn_primitive_id_arr bias;
/// @brief Indicates that deconvolution is used for convolution backward computation (convolution_grad_input)
uint32_t gradient;
/// @brief Number of feature groups (grouped deconvolution). If more than 1 then weights/bias count needs to be 1.
uint32_t groups;
CLDNN_END_PRIMITIVE_DESC(deconvolution)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(deconvolution);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* DECONVOLUTION_H */

