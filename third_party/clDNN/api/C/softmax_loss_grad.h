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
#ifndef SOFTMAX_LOSS_GRAD_H
#define SOFTMAX_LOSS_GRAD_H

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

/// @brief Backward pass for Softmax log loss.
/// @details The output values are the same as input_prob, except for the correct one based on the label which is subtracted by 1.
CLDNN_BEGIN_PRIMITIVE_DESC(softmax_loss_grad)

CLDNN_END_PRIMITIVE_DESC(softmax_loss_grad)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(softmax_loss_grad);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* SOFTMAX_LOSS_GRAD_H */

