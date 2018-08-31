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
#ifndef DATA_H
#define DATA_H

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

/// @brief Provides input data to topology.
/// @details This primitive allows to pass data which is known at topology creation (constants).
/// For example, weights and biases for scoring networks.
/// @note Passing data at topology may improve network performance if data optimization is enabled.
CLDNN_BEGIN_PRIMITIVE_DESC(data)
/// @brief Memory object which contains data.
/// @note If memory is attached by ::cldnn_attach_memory(),
/// attached buffer should be valid on ::cldnn_build_network() call.
cldnn_memory mem;
CLDNN_END_PRIMITIVE_DESC(data)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(data);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* DATA_H */

