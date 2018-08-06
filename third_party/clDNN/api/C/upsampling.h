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
#ifndef upsampling_H
#define upsampling_H

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

/// @brief Sample mode for upsampling layer ( @CLDNN_PRIMITIVE_DESC{upsampling} ​).
typedef enum /*:int32_t*/
{
    /// @brief upsampling nearest neighbor.
    cldnn_upsampling_nearest,
    /// @brief upsampling bilinear.
    cldnn_upsampling_bilinear,
} cldnn_upsampling_sample_type;

/// @brief Performs nearest neighbor/bilinear upsampling
/// Also supports built-in Relu @ref activation available by setting it in arguments.
CLDNN_BEGIN_PRIMITIVE_DESC(upsampling)
/// @param scale Upsampling scale.
uint32_t scale;
/// @param num_filter Input filter. Only used by bilinear sample_type.
uint32_t num_filter;
/// @param sample_type Upsampling method (nearest neighbor/bilinear).
int32_t sample_type; /*cldnn_sample_type*/
/// @brief Enables Relu activation.
uint32_t with_activation;
/// @brief Relu activation slope.
float activation_negative_slope;
CLDNN_END_PRIMITIVE_DESC(upsampling)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(upsampling);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* upsampling_H */

