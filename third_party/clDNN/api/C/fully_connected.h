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
#ifndef FULLY_CONNECTED_H
#define FULLY_CONNECTED_H

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

/// @brief Performs forward fully connected layer (inner product).
/// Also supports built-in Relu @CLDNN_PRIMITIVE_DESC{activation} available by setting it in arguments.
CLDNN_BEGIN_PRIMITIVE_DESC(fully_connected)
/// @brief Enable Relu activation.
uint32_t with_activation;
/// @brief Relu activation slope.
float activation_negative_slope;
/// @brief Primitive id containing weights data.
cldnn_primitive_id weights;
/// @brief Primitive id containing bias data.
cldnn_primitive_id bias;
/// @brief Primitive id containing weights quanitization factors per output feature map.
cldnn_primitive_id weights_quantization_factors;
/// @brief Primitive id containing output quanitization factors per output feature map.
cldnn_primitive_id output_calibration_factors;
/// @brief Input quantization factor
float input_quantization_factor;
/// @brief Output quantization factor
float output_quantization_factor;

CLDNN_END_PRIMITIVE_DESC(fully_connected)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(fully_connected);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* FULLY_CONNECTED_H */

