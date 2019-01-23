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
#ifndef DETECTION_OUTPUT_SORT_H
#define DETECTION_OUTPUT_SORT_H

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

    /// @brief Generates a list of detections based on location and confidence predictions by doing non maximum suppression.
    /// @details Each row is a 7 dimension vector, which stores: [image_id, label, confidence, xmin, ymin, xmax, ymax].
    /// If number of detections per image is lower than keep_top_k, will write dummy results at the end with image_id=-1. 
    CLDNN_BEGIN_PRIMITIVE_DESC(detection_output_sort)
        /// @brief Number of classes to be predicted.
        uint32_t num_classes;
    /// @brief Number of classes to be predicted.
    uint32_t num_images;
    /// @brief Number of total bounding boxes to be kept per image after NMS step.
    uint32_t keep_top_k;
    /// @brief If true, bounding box are shared among different classes.
    uint32_t share_location;
    /// @brief Maximum number of results to be kept in NMS.
    int top_k;
    /// @brief Background label id (-1 if there is no background class).
    int background_label_id;
    CLDNN_END_PRIMITIVE_DESC(detection_output_sort)

    CLDNN_DECLARE_PRIMITIVE_TYPE_ID(detection_output_sort);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* DETECTION_OUTPUT_SORT_H */
