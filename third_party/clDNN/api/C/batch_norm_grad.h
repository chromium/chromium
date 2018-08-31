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
#ifndef BATCH_NORM_GRAD_H
#define BATCH_NORM_GRAD_H

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

/// @brief Performs backward batch normalization layer.
/// @details Calculates mean gradient and gradient * input for every feature in data, 
/// then output is calculated as inv_variance * (input_grad - mean_grad_input * input - mean_grad)
CLDNN_BEGIN_PRIMITIVE_DESC(batch_norm_grad)
/// @brief Primitive id containing inverted variance from forward pass.
cldnn_primitive_id inv_variance;
CLDNN_END_PRIMITIVE_DESC(batch_norm_grad)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(batch_norm_grad);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* BATCH_NORM_GRAD_H */

