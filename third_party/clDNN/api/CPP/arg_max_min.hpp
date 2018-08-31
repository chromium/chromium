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
#pragma once
#include "../C/arg_max_min.h"
#include "primitive.hpp"

namespace cldnn
{
    /// @addtogroup cpp_api C++ API
    /// @{
    /// @addtogroup cpp_topology Network Topology
    /// @{
    /// @addtogroup cpp_primitives Primitives
    /// @{

    /// @brief Finds the index of the k max values of input.
    /// @details Returns indices in f32, because we currently does not support int32 data type. 
    /// We use f32, as bigger indices could not fit in smaller data types.
    /// If you want to use output as indices outside of network (inside just use lookup table primitive), 
    /// you will need to firstly cast it to int (look into tests for example).
    struct arg_max_min : public primitive_base<arg_max_min, CLDNN_PRIMITIVE_DESC(arg_max_min)>
    {
        CLDNN_DECLARE_PRIMITIVE(arg_max_min)

        /// @brief Enum type to specify axis to return values from.
        enum out_type
        {
            max,
            min,
        };

        /// @brief Enum type to specify axis to maximize/minimize along.
        enum axis_name
        {
            batch,
            feature,
            x,
            y,
            xyf
        };

        /// @brief Constructs arg_max_min primitive.
        /// @param id This primitive id.
        /// @param input Input primitive id.
        /// @param out_type Type of output - max or mix.
        /// @param top_k Number of indices to output.
        /// @param axis Axis to maximize/minimize along.
        arg_max_min(
            const primitive_id& id,
            const primitive_id& input,
            out_type output_type,
            uint32_t top_k = 1,
            axis_name axis = axis_name::xyf,
            const padding& output_padding = padding()
        )
            :primitive_base(id, { input }, output_padding)
            , top_k(top_k)
            , output_type(output_type)
            , axis(axis)
            , with_axis(axis == axis_name::xyf ? false : true)
        {}

        /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{arg_max_min}
        arg_max_min(const dto* dto)
            :primitive_base(dto)
            , top_k(dto->top_k)
            , output_type(static_cast<out_type>(dto->output_type))
            , axis(static_cast<axis_name>(dto->axis))
            , with_axis(dto->with_axis != 0)
        {}

        /// @brief Number of indices to output.
        uint32_t top_k;
        /// @brief Type of output - max or mix.
        out_type output_type;
        /// @brief Axis to maximize/minimize along. If not set, maximize the flattened trailing dimensions for each index of the batch dimension.
        axis_name axis;
        /// @brief Indicates that the primitive has user defined axis to maximize/minimize along;
        bool with_axis;
        

    protected:

        void update_dto(dto& dto) const override
        {
            dto.top_k = top_k;
            dto.output_type = static_cast<cldnn_arg_max_min_out>(output_type);
            dto.with_axis = with_axis;
            dto.axis = static_cast<cldnn_arg_max_min_axis>(axis);
        }
    };
    /// @}
    /// @}
    /// @}
}