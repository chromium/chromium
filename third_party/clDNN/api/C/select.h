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
#ifndef SELECT_H
#define SELECT_H

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

/// @brief Performs elementwise select operation on two input primitives with selector primitive (mask)
/// @notes
/// - both inputs have to have equal sizes in all dimensions
/// - format of both inputs has to be the same
/// - mask primitive input have to have equal size in all dimensions with inputs
CLDNN_BEGIN_PRIMITIVE_DESC(select)

CLDNN_END_PRIMITIVE_DESC(select)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(select);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* SELECT_H */

