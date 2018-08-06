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
#include "../C/lookup_table.h"
#include "primitive.hpp"

namespace cldnn
{
    /// @addtogroup cpp_api C++ API
    /// @{
    /// @addtogroup cpp_topology Network Topology
    /// @{
    /// @addtogroup cpp_primitives Primitives
    /// @{

    /// @brief Returns values from data on which given indices are pointing at.
    struct lookup_table : public primitive_base<lookup_table, CLDNN_PRIMITIVE_DESC(lookup_table)>
    {
        CLDNN_DECLARE_PRIMITIVE(lookup_table)

        /// @brief Enum type to specify axis to maximize/minimize along.
        enum axis_name
        {
            batch,
            feature,
            x,
            y,
            xyf
        };

        /// @brief Constructs lookup_table primitive.
        /// @param id This primitive id.
        /// @param input_data Input data primitive id.
        /// @param input_indices Input indices primitive id.
        /// @param axis Axis to return values from.
        lookup_table(
            const primitive_id& id,
            const primitive_id& input_data,
            const primitive_id& input_indices,
            axis_name axis = axis_name::xyf,
            const padding& output_padding = padding()
        )
            :primitive_base(id, { input_data, input_indices }, output_padding)
            , axis(axis)
            , with_axis(axis == axis_name::xyf ? false : true)
        {}

        /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{lookup_table}
        lookup_table(const dto* dto)
            :primitive_base(dto)
            , axis(static_cast<axis_name>(dto->axis))
            , with_axis(dto->with_axis != 0)
        {}

        /// @brief Axis to return values from. If not set, returns data which index is pointing at in the flattened x, y, f dimensions for each batch.
        axis_name axis;
        /// @brief Indicates that the primitive has user defined axis to return values from.
        bool with_axis;

    protected:

        void update_dto(dto& dto) const override
        {
            dto.with_axis = with_axis;
            dto.axis = static_cast<cldnn_lookup_table_axis>(axis);
        }
    };
    /// @}
    /// @}
    /// @}
}