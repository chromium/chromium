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
#ifndef DETECTION_OUTPUT_H
#define DETECTION_OUTPUT_H

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

/// @brief Select method for coding the prior-boxes in Detection Output layer ( @CLDNN_PRIMITIVE_DESC{detection_output} ).
typedef enum /*:int32_t*/
{
    cldnn_code_type_corner,
    cldnn_code_type_center_size,
    cldnn_code_type_corner_size,
} cldnn_prior_box_code_type;

/// @brief Generates a list of detections based on location and confidence predictions by doing non maximum suppression.
/// @details Each row is a 7 dimension vector, which stores: [image_id, label, confidence, xmin, ymin, xmax, ymax].
/// If number of detections per image is lower than keep_top_k, will write dummy results at the end with image_id=-1. 
CLDNN_BEGIN_PRIMITIVE_DESC(detection_output)
/// @brief Number of classes to be predicted.
uint32_t num_classes;
/// @brief Number of total bounding boxes to be kept per image after NMS step.
uint32_t keep_top_k;
/// @brief If not 0, bounding box are shared among different classes.
uint32_t share_location;
/// @brief Background label id (-1 if there is no background class).
int background_label_id;
/// @brief Threshold for NMS step.
float nms_threshold;
/// @brief Maximum number of results to be kept in NMS.
int top_k;
/// @brief Used for adaptive NMS.
float eta;
/// @brief Type of coding method for bounding box. See #cldnn_prior_box_code_type.
int32_t code_type;
/// @brief If not 0, variance is encoded in target; otherwise we need to adjust the predicted offset accordingly.
uint32_t variance_encoded_in_target;
/// @brief Only keep detections with confidences larger than this threshold.
float confidence_threshold;
/// @brief Number of elements in a single prior description (4 if priors calculated using PriorBox layer, 5 - if Proposal)
int32_t prior_info_size;
/// @brief Offset of the box coordinates w.r.t. the beginning of a prior info record
int32_t prior_coordinates_offset;
/// @brief If true, priors are normalized to [0; 1] range.
uint32_t prior_is_normalized;
/// @brief Width of input image.
int32_t input_width;
/// @brief Height of input image.
int32_t input_height;
/// @brief Decrease label id to skip background label equal to 0. Can't be used simultaneously with background_label_id.
int32_t decrease_label_id;
/// @brief Clip decoded boxes
int32_t clip;
CLDNN_END_PRIMITIVE_DESC(detection_output)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(detection_output);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* DETECTION_OUTPUT_H */
