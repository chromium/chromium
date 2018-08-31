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
#ifndef PRIOR_BOX_H
#define PRIOR_BOX_H

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

/// @brief Generates a set of default bounding boxes with different sizes and aspect ratios.
/// @details The prior-boxes are shared across all the images in a batch (since they have the same width and height).
/// First feature stores the mean of each prior coordinate. 
/// Second feature stores the variance of each prior coordinate.
CLDNN_BEGIN_PRIMITIVE_DESC(prior_box) 
/// @brief Image width and height.
cldnn_tensor img_size;
/// @brief Minimum box sizes in pixels.
cldnn_float_arr min_sizes;
/// @brief Maximum box sizes in pixels.
cldnn_float_arr max_sizes;
/// @brief Various of aspect ratios. Duplicate ratios will be ignored.
cldnn_float_arr aspect_ratios;
/// @brief If not 0, will flip each aspect ratio. For example, if there is aspect ratio "r", aspect ratio "1.0/r" we will generated as well.
uint32_t flip;
/// @brief If not 0, will clip the prior so that it is within [0, 1].
uint32_t clip;
/// @brief Variance for adjusting the prior boxes.
cldnn_float_arr variance;
/// @brief Step width.
float step_width;
/// @brief Step height.
float step_height;
/// @brief Offset to the top left corner of each cell.
float offset;
/// @broef If false, only first min_size is scaled by aspect_ratios
uint32_t scale_all_sizes;
CLDNN_END_PRIMITIVE_DESC(prior_box)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(prior_box);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* PRIOR_BOX_H */

