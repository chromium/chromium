/*
// Copyright (c) 2017 Intel Corporation
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
#ifndef RESHAPE_H
#define RESHAPE_H

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

/// @brief Changes information about inputs's layout effectively creating new memory which share underlaying buffer
/// but is interpreted in a different way (different shape).
/// @note reshape primitive is supposed only to reinterpret shape of the memory therefore it's not possible to change
/// neither data type nor format of the input buffer and total number of elements in input and output (excluding paddings) must match.
/// Please note that there is no guarantee that underlying data will be in proper format if primitive was explicitly added to output list.
CLDNN_BEGIN_PRIMITIVE_DESC(reshape)
/// @brief Requested memory shape.
cldnn_tensor output_shape;
CLDNN_END_PRIMITIVE_DESC(reshape)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(reshape);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* RESHAPE_H */
