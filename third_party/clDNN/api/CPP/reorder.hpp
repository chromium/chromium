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
#include "../C/reorder.h"
#include "primitive.hpp"
#include "memory.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Changes how data is ordered in memory. Value type is not changed & all information is preserved.
/// @details Corresponding values are bitwise equal before/after reorder.
/// Also merged with subtraction layer, which can subtract, multiply or divide values based on mean_mode value, while doing reordering.
/// NOTE THAT THIS WILL SUBTRACT THE SAME VALUES FROM EACH BATCH.
struct reorder : public primitive_base<reorder, CLDNN_PRIMITIVE_DESC(reorder)>
{
    CLDNN_DECLARE_PRIMITIVE(reorder)

    /// @brief Constructs reorder primitive with directly provided mean subtract values.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_layout Requested memory layout.
    /// @param values_to_subtract Array of mean subtract values.
    reorder(
        const primitive_id& id,
        const primitive_id& input,
        const layout& output_layout,
        const std::vector<float>& values_to_subtract = {},
        const cldnn_reorder_mean_mode mode = cldnn_reorder_mean_mode::mean_subtract
    )
        : primitive_base(id, { input }, output_layout.data_padding)
        , output_format(output_layout.format)
        , output_data_type(output_layout.data_type)
        , mean("")
        , subtract_per_feature(values_to_subtract)
        , mean_mode(mode)
    {
    }

    /// @brief Constructs reorder primitive which takes mean subtract values from another primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_layout Requested memory layout.
    /// @param mean Primitive id to get mean subtract values.
    reorder(
        const primitive_id& id,
        const primitive_id& input,
        const layout& output_layout,
        primitive_id const& mean,
        const cldnn_reorder_mean_mode mode = cldnn_reorder_mean_mode::mean_subtract
    )
        : primitive_base(id, { input }, output_layout.data_padding)
        , output_format(output_layout.format)
        , output_data_type(output_layout.data_type)
        , mean(mean)
        , subtract_per_feature(0)
        , mean_mode(mode)
    {
    }

    /// @brief Constructs reorder primitive with directly provided mean subtract values.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_layout Requested memory layout.
    /// @param values_to_subtract Array of mean subtract values.
    reorder(
        const primitive_id& id,
        const primitive_id& input,
        format output_format,
        data_types output_data_type,
        const std::vector<float>& values_to_subtract = {},
        const cldnn_reorder_mean_mode mode = cldnn_reorder_mean_mode::mean_subtract,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input }, output_padding)
        , output_format(output_format)
        , output_data_type(output_data_type)
        , mean("")
        , subtract_per_feature(values_to_subtract)
        , mean_mode(mode)
    {
    }

    /// @brief Constructs reorder primitive which takes mean subtract values from another primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_layout Requested memory layout.
    /// @param mean Primitive id to get mean subtract values.
    reorder(
        const primitive_id& id,
        const primitive_id& input,
        format output_format,
        data_types output_data_type,
        primitive_id const& mean,
        const cldnn_reorder_mean_mode mode = cldnn_reorder_mean_mode::mean_subtract,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input }, output_padding)
        , output_format(output_format)
        , output_data_type(output_data_type)
        , mean(mean)
        , subtract_per_feature(0)
        , mean_mode(mode)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{reorder}
    reorder(const dto* dto)
        : primitive_base(dto)
        , output_format(dto->output_format)
        , output_data_type(static_cast<data_types>(dto->output_data_type))
        , mean(dto->mean_subtract)
        , subtract_per_feature(float_arr_to_vector(dto->subtract_per_feature))
        , mean_mode(dto->mean_mode)
    {
    }

    /// @brief Requested memory format.
    format output_format;
    /// @brief Requested memory data type.
    data_types output_data_type;
    /// @brief Primitive id to get mean subtract values. Ignored if subtract_per_featrue is set.
    primitive_id mean;
    /// @brief Array of mean subtract values.
    std::vector<float> subtract_per_feature;
    /// @brief Mode of mean execution
    cldnn_reorder_mean_mode mean_mode;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        if (mean.empty())
            return{};
        return{ mean };
    }

    void update_dto(dto& dto) const override
    {
        dto.output_format = static_cast<cldnn_format_type>(output_format.value);
        dto.output_data_type = static_cast<cldnn_data_type>(output_data_type);
        dto.mean_subtract = mean.c_str();
        dto.subtract_per_feature = float_vector_to_arr(subtract_per_feature);
        dto.mean_mode = mean_mode;
    }
};
/// @}
/// @}
/// @}
}
