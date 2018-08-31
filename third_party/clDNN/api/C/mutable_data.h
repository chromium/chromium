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
#ifndef MUTABLE_DATA_H
#define MUTABLE_DATA_H

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

/// @brief Enum type to specify function for weights filling.
typedef enum
{
    zero,
    xavier
} cldnn_filler_type;

/// @brief Provides mutable data.
/// @details This primitive allows to pass data which can be written to during training.
/// For example, weights and biases for scoring networks.
/// This primitive can be also set as other primitive's output. In this case the underlying buffer will be the same in mutable_data and preceding primitive.
CLDNN_BEGIN_PRIMITIVE_DESC(mutable_data)
/// @brief Memory object which contains data.
/// @note If memory is attached by ::cldnn_attach_memory(),
/// attached buffer should be valid on ::cldnn_build_network() call.
cldnn_memory mem;
/// @brief Specifies function which will be used to fill data.
cldnn_filler_type fill_type;
CLDNN_END_PRIMITIVE_DESC(mutable_data)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(mutable_data);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* MUTABLE_DATA_H */

