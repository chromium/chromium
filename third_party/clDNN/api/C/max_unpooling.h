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
#ifndef MAX_UNPOOLING_H
#define MAX_UNPOOLING_H

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

/// @brief Performs "max_unpooling" operation.
/// @details Reverse operation of max pooling, based on the argmax data where indices of each max pooling region are stored.
CLDNN_BEGIN_PRIMITIVE_DESC(max_unpooling)
/// @brief Primitive id which contains indices of each max pooling region. Indices must be in flattened bfyx format with no padding. Needs to be fp32 data type.
cldnn_primitive_id argmax;
/// @brief Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the pooling window should start calculations. Used only for output size computation.
cldnn_tensor input_offset;
/// @brief Defines shift in input buffer between adjacent calculations of output values. Used only for output size computation.
cldnn_tensor stride;
/// @brief Pooling kernel size. Used only for output size computation.
cldnn_tensor size;
/// @brief Indicates that the primitive has user-defined output size (non-zero value).
uint32_t with_output_size;
/// @brief User-defined output data size of the primitive (w/o padding).
cldnn_tensor output_size;
CLDNN_END_PRIMITIVE_DESC(max_unpooling)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(max_unpooling);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* MAX_UNPOOLING_H */

