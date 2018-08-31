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
#ifndef fully_connected_grad_weights_GRAD_WEIGHTS_H
#define fully_connected_grad_weights_GRAD_WEIGHTS_H

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

/// @brief Performs backward fully connected layer (inner product) for weights and biases.
CLDNN_BEGIN_PRIMITIVE_DESC(fully_connected_grad_weights)
/// @brief Primitive id containing weights data.
cldnn_primitive_id weights;
/// @brief Primitive id containing bias data.
cldnn_primitive_id bias;
/// @brief Primitive id containing fully connected gradient data. Used for proper order of gradient calculation. Leave empty if primitive is last in backward pass.
cldnn_primitive_id fc_grad;
/// @brief Primitive id containing weight gradient calculated in previous iteration. Memory size should be same as weights.
cldnn_primitive_id prev_weights_grad;
/// @brief Primitive id containing bias gradient calculated in previous iteration. Memory size should be same as bias.
cldnn_primitive_id prev_bias_grad;
CLDNN_END_PRIMITIVE_DESC(fully_connected_grad_weights)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(fully_connected_grad_weights);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* fully_connected_grad_weights_GRAD_WEIGHTS_H */

