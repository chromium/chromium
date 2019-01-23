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
#ifndef BATCH_NORM_H
#define BATCH_NORM_H

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

/// @brief Batch normalization primitive.
/// @details Performs batch normalization as described in
/// "Batch Normalization: Accelerating Deep Network Training by Reducing Internal Covariate Shift" by Ioffe, Szegedy
/// @n See: http://arxiv.org/abs/1502.03167
/// 
/// <b>Algorithm:</b>
/// @n global stats can be computed as:
/// @n out[i] = ( (in[i] - mean[b]) / sqrt(variance[b] + epsilon) ) * scale[b] + shift[b]

CLDNN_BEGIN_PRIMITIVE_DESC(batch_norm)
/// @brief Primitive id containing mean data.
cldnn_primitive_id mean;
/// @brief Primitive id containing variance.
cldnn_primitive_id variance;
/// @brief Primitive id containing scale.
cldnn_primitive_id scale;
/// @brief Primitive id containing shift.
cldnn_primitive_id shift;
/// @brief Primitive id containing inverted variance used in future gradient computing.
cldnn_primitive_id inv_variance;
/// @brief Epsilon.
float epsilon;
CLDNN_END_PRIMITIVE_DESC(batch_norm)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(batch_norm);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* BATCH_NORM_H */

