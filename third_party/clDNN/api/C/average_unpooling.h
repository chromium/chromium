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
#ifndef AVERAGE_UNPOOLING_H
#define AVERAGE_UNPOOLING_H

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

/// @brief Performs "average_unpooling" operation.
/// @details Reverse operation of average pooling.
/// Each element in every pooling window is filled with output / window size value. In case of window overlap the elements are added.
CLDNN_BEGIN_PRIMITIVE_DESC(average_unpooling)
/// @brief Defines shift in output buffer.
cldnn_tensor stride;
/// @brief Pooling kernel size.
cldnn_tensor size;
/// @brief Output size of this primitive.
cldnn_tensor output_size;
CLDNN_END_PRIMITIVE_DESC(average_unpooling)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(average_unpooling);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* AVERAGE_UNPOOLING_H */

