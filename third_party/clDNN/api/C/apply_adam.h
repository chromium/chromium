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
#ifndef APPLY_ADAM_H
#define APPLY_ADAM_H

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

/// @brief Apply Adam primitive.
/// @details Updates output using Adam algorithm. The output of this primitive should be mutable_data type in case user wants to update
/// variable accross network. If output is not mutable_data then it will be initialized with 0.
/// "Adam: A Method for Stochastic Optimization" by Diederik P. Kingma, Jimmy Ba
/// @n See: https://arxiv.org/abs/1412.6980
/// 
/// <b>Algorithm:</b>
/// @n float lr[t] = lr * sqrt(1 - beta2^t) / (1 - beta1^t);
/// @n float m[t] = beta1 * m[t-1] + (1 - beta1) * grad[t];
/// @n float v[t] = beta2 * v[t-1] + (1 - beta2) * grad[t] * grad[t];
/// @n float result = result - lr[t] * m[t] / (sqrt(v[t]) + epsilon);

CLDNN_BEGIN_PRIMITIVE_DESC(apply_adam)
/// @brief Primitive id containing m data.
cldnn_primitive_id m;
/// @brief Primitive id containing v data.
cldnn_primitive_id v;
/// @brief Primitive id containing beta1^t.
cldnn_primitive_id beta1_power;
/// @brief Primitive id containing beta2^t.
cldnn_primitive_id beta2_power;
/// @brief Learning rate parameter.
float lr;
/// @brief Beta1 parameter.
float beta1;
/// @brief Beta2 parameter.
float beta2;
/// @brief Epsilon.
float epsilon;
CLDNN_END_PRIMITIVE_DESC(apply_adam)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(apply_adam);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* APPLY_ADAM_H */

