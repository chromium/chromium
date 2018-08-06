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
#ifndef permute_H
#define permute_H

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

/// @brief Permutes data in the memory, with respect to provided order.
/// @details Permute order is set as vector with positions meaning corresponding to tensor.
/// Vector values represent dimensions to be permuted in bfyx format. For example: <br>
/// input_dimensions = tensor{ 5, 3, 6, 3 } <br>
/// permute_order = { 2, 3, 1, 0 } <br>
/// output_dimensions = { 6, 3, 3, 5 } <br>
/// <br>
/// When permute_order is { 0, 1, 2, 3 } then input_dimensions = output_dimensions
CLDNN_BEGIN_PRIMITIVE_DESC(permute)
/// @brief Array of permuted output order in bfyx format.
cldnn_uint16_t_arr permute_order;
CLDNN_END_PRIMITIVE_DESC(permute)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(permute);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* permute_H */

