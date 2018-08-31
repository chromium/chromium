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
#pragma once

#include <cmath>

#include "../C/prior_box.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Generates a set of default bounding boxes with different sizes and aspect ratios.
/// @details The prior-boxes are shared across all the images in a batch (since they have the same width and height).
/// First feature stores the mean of each prior coordinate. 
/// Second feature stores the variance of each prior coordinate.
struct prior_box : public primitive_base<prior_box, CLDNN_PRIMITIVE_DESC(prior_box)>
{
    CLDNN_DECLARE_PRIMITIVE(prior_box)

    /// @brief Constructs prior-box primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param img_size Image width and height.
    /// @param min_sizes Minimum box sizes in pixels.
    /// @param max_sizes Maximum box sizes in pixels.
    /// @param aspect_ratios Various of aspect ratios. Duplicate ratios will be ignored.
    /// @param flip If true, will flip each aspect ratio. For example, if there is aspect ratio "r", aspect ratio "1.0/r" we will generated as well.
    /// @param clip If true, will clip the prior so that it is within [0, 1].
    /// @param variance Variance for adjusting the prior boxes.
    /// @param step_width Step width.
    /// @param step_height Step height.
    /// @param offset Offset to the top left corner of each cell.
    prior_box(
        const primitive_id& id,
        const primitive_id& input,
        const tensor& img_size,
        const std::vector<float>& min_sizes,
        const std::vector<float>& max_sizes = {},
        const std::vector<float>& aspect_ratios = {},
        const bool flip = true,
        const bool clip = false,
        const std::vector<float>& variance = {},
        const float step_width = 0.f,
        const float step_height = 0.f,
        const float offset = 0.5f,
        const bool scale_all_sizes = true,
        const padding& output_padding = padding()
        )
        : primitive_base(id, {input}, output_padding)
        , img_size(img_size)
        , min_sizes(min_sizes)
        , max_sizes(max_sizes)
        , flip(flip)
        , clip(clip)
        , step_width(step_width)
        , step_height(step_height)
        , offset(offset)
        , scale_all_sizes(scale_all_sizes)
    {
        this->aspect_ratios.push_back(1.f);
        for (auto new_aspect_ratio : aspect_ratios) {
            bool already_exist = false;
            for (auto aspect_ratio : this->aspect_ratios) {
                if (std::fabs(new_aspect_ratio - aspect_ratio) < 1e-6) {
                    already_exist = true;
                    break;
                }
            }
            if (!already_exist) {
                this->aspect_ratios.push_back(new_aspect_ratio);
                if (flip) {
                    this->aspect_ratios.push_back(1.f / new_aspect_ratio);
                }
            }
        }
        if (variance.size() > 1) {
            for (size_t i = 0; i < variance.size(); ++i) {
                this->variance.push_back(variance[i]);
            }
        }
        else if (variance.size() == 1) {
            this->variance.push_back(variance[0]);
        }
        else {
            // Set default to 0.1.
            this->variance.push_back(0.1f);
        }
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{prior-box}
    prior_box(const dto* dto)
        : primitive_base(dto)
        , img_size(dto->img_size)
        , min_sizes(float_arr_to_vector(dto->min_sizes))
        , max_sizes(float_arr_to_vector(dto->max_sizes))
        , aspect_ratios(float_arr_to_vector(dto->aspect_ratios))
        , flip(dto->flip != 0)
        , clip(dto->clip != 0)
        , variance(float_arr_to_vector(dto->variance))
        , step_width(dto->step_width)
        , step_height(dto->step_height)
        , offset(dto->offset)
        , scale_all_sizes(dto->scale_all_sizes != 0)
    {}

    /// @brief Image width and height.
    tensor img_size;
    /// @brief  Minimum box sizes in pixels.
    std::vector<float> min_sizes;
    /// @brief Maximum box sizes in pixels.
    std::vector<float> max_sizes;
    /// @brief Various of aspect ratios. Duplicate ratios will be ignored.
    std::vector<float> aspect_ratios;
    /// @brief If true, will flip each aspect ratio. For example, if there is aspect ratio "r", aspect ratio "1.0/r" we will generated as well.
    bool flip;
    /// @brief If true, will clip the prior so that it is within [0, 1].
    bool clip;
    /// @brief Variance for adjusting the prior boxes.
    std::vector<float> variance;
    /// @brief Step width.
    float step_width;
    /// @brief Step height.
    float step_height;
    /// @brief Offset to the top left corner of each cell.
    float offset;
    /// @broef If false, only first min_size is scaled by aspect_ratios
    bool scale_all_sizes;

private:
    void update_dto(dto& dto) const override
    {
        dto.img_size = img_size;
        dto.min_sizes = float_vector_to_arr(min_sizes);
        dto.max_sizes = float_vector_to_arr(max_sizes);
        dto.aspect_ratios = float_vector_to_arr(aspect_ratios);
        dto.flip = flip;
        dto.clip = clip;
        dto.variance = float_vector_to_arr(variance);
        dto.step_width = step_width;
        dto.step_height = step_height;
        dto.offset = offset;
        dto.scale_all_sizes = scale_all_sizes;
    }

    static cldnn_float_arr float_vector_to_arr(const std::vector<float>& stor)
    {
        return{ stor.data(), stor.size() };
    }

    static std::vector<float> float_arr_to_vector(const cldnn_float_arr& arr)
    {
        std::vector<float> result(arr.size);
        for (size_t i = 0; i < arr.size; i++)
        {
            result[i] = arr.data[i];
        }
        return result;
    }
};
/// @}
/// @}
/// @}
}