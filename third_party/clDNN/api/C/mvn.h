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
#ifndef MVN_H
#define MVN_H

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

/// @brief Mean Variance Normalization primitive.
/// @details Normalizes the input to have 0-mean and/or unit (1) variance.

CLDNN_BEGIN_PRIMITIVE_DESC(mvn)
/// @brief Determines if the normalization is done across or within channels.
uint32_t across_channels;
/// @brief Determines if normalize variance is applied.
uint32_t normalize_variance;
/// @brief Epsilon for not dividing by zero while normalizing.
float epsilon;
CLDNN_END_PRIMITIVE_DESC(mvn)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(mvn);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* MVN_H */

