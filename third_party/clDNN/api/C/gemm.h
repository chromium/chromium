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
#ifndef GEMM_H
#define GEMM_H

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

/// @brief Performs forward attention layer.

CLDNN_BEGIN_PRIMITIVE_DESC(gemm)
/// @brief Variable containing ALPHA parameter
float alpha;
/// @brief Variable containing BETA parameter
float beta;
/// @brief Flag for transposing first input matrix
bool transpose_input1;
/// @brief Flag for transposing second input matrix
bool transpose_input2;
CLDNN_END_PRIMITIVE_DESC(gemm)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(gemm);


#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* GEMM_H */

