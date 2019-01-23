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
#pragma once

#include <vector>

#include "../C/proposal.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

struct proposal : public primitive_base<proposal, CLDNN_PRIMITIVE_DESC(proposal)>
{
    CLDNN_DECLARE_PRIMITIVE(proposal)
 
    proposal(
        const primitive_id& id,        
        const primitive_id& cls_scores,
        const primitive_id& bbox_pred,
        const primitive_id& image_info,
        int max_proposals,
        float iou_threshold,
        int min_bbox_size,
        int feature_stride,
        int pre_nms_topn,
        int post_nms_topn,
        const std::vector<float>& ratios_param,
        const std::vector<float>& scales_param,
        const padding& output_padding = padding()
        )
        : primitive_base(id, {cls_scores, bbox_pred, image_info}, output_padding),
                 max_proposals(max_proposals),
                 iou_threshold(iou_threshold),
                 base_bbox_size(16),
                 min_bbox_size(min_bbox_size),
                 feature_stride(feature_stride),
                 pre_nms_topn(pre_nms_topn),
                 post_nms_topn(post_nms_topn),
                 ratios(ratios_param),
                 scales(scales_param),
                 coordinates_offset(1.0f),
                 box_coordinate_scale(1.0f),
                 box_size_scale(1.0f),
                 swap_xy(false),
                 initial_clip(false),
                 round_ratios(true),
                 shift_anchors(false)
    {
    }

    proposal(
        const primitive_id& id,
        const primitive_id& cls_scores,
        const primitive_id& bbox_pred,
        const primitive_id& image_info,
        int max_proposals,
        float iou_threshold,
        int base_bbox_size,
        int min_bbox_size,
        int feature_stride,
        int pre_nms_topn,
        int post_nms_topn,
        const std::vector<float>& ratios_param,
        const std::vector<float>& scales_param,
        float coordinates_offset,
        float box_coordinate_scale,
        float box_size_scale,
        bool swap_xy,
        bool initial_clip,
        bool round_ratios,
        bool shift_anchors,
        const padding& output_padding = padding()
        )
        : primitive_base(id, {cls_scores, bbox_pred, image_info}, output_padding),
                 max_proposals(max_proposals),
                 iou_threshold(iou_threshold),
                 base_bbox_size(base_bbox_size),
                 min_bbox_size(min_bbox_size),
                 feature_stride(feature_stride),
                 pre_nms_topn(pre_nms_topn),
                 post_nms_topn(post_nms_topn),
                 ratios(ratios_param),
                 scales(scales_param),
                 coordinates_offset(coordinates_offset),
                 box_coordinate_scale(box_coordinate_scale),
                 box_size_scale(box_size_scale),
                 swap_xy(swap_xy),
                 initial_clip(initial_clip),
                 round_ratios(round_ratios),
                 shift_anchors(shift_anchors)
    {
    }

    proposal(const dto* dto) :
        primitive_base(dto),
        max_proposals(dto->max_proposals),
        iou_threshold(dto->iou_threshold),
        base_bbox_size(dto->base_bbox_size),
        min_bbox_size(dto->min_bbox_size),
        feature_stride(dto->feature_stride),
        pre_nms_topn(dto->pre_nms_topn),
        post_nms_topn(dto->post_nms_topn),
        ratios(float_arr_to_vector(dto->ratios)),
        scales(float_arr_to_vector(dto->scales)),
        coordinates_offset(dto->coordinates_offset),
        box_coordinate_scale(dto->box_coordinate_scale),
        box_size_scale(dto->box_size_scale),
        swap_xy(dto->swap_xy != 0),
        initial_clip(dto->initial_clip != 0),
        round_ratios(dto->round_ratios != 0),
        shift_anchors(dto->shift_anchors != 0)
    {
    }

    int max_proposals;
    float iou_threshold;
    int base_bbox_size;
    int min_bbox_size;
    int feature_stride;
    int pre_nms_topn;
    int post_nms_topn;      
    std::vector<float> ratios;
    std::vector<float> scales;
    float coordinates_offset;
    float box_coordinate_scale;
    float box_size_scale;
    bool swap_xy;
    bool initial_clip;
    bool round_ratios;
    bool shift_anchors;

protected:
    void update_dto(dto& dto) const override
    {
        dto.max_proposals = max_proposals;
        dto.iou_threshold = iou_threshold;
        dto.base_bbox_size = base_bbox_size;
        dto.min_bbox_size = min_bbox_size;
        dto.feature_stride = feature_stride;
        dto.pre_nms_topn = pre_nms_topn;
        dto.post_nms_topn = post_nms_topn;
        dto.ratios = float_vector_to_arr(ratios);
        dto.scales = float_vector_to_arr(scales);
        dto.coordinates_offset = coordinates_offset;
        dto.box_coordinate_scale = box_coordinate_scale;
        dto.box_size_scale = box_size_scale;
        dto.swap_xy = swap_xy;
        dto.initial_clip = initial_clip;
        dto.round_ratios = round_ratios;
        dto.shift_anchors = shift_anchors;
    }
};

/// @}
/// @}
/// @}
}
