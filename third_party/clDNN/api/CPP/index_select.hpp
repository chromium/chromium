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

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../C/index_select.h"
#include "primitive.hpp"


namespace cldnn
{
/// @brief Select index, which will be copied to the output..
///
/// @details Applies index selecting along specified dimension. The indices, which will be copied are specifed by 
///          by @c indices.
/// @n
/// @n Example:
/// @n      <tt>input_sizes  = (1, 2, 4, 2)</tt>
/// @n      <tt>input_values = (a, b, c, d)</tt>
/// @n      <tt>               (e, f, g, h)</tt>
/// @n      <tt>indices_sizes  = (1, 1, 6, 1)</tt>
/// @n      <tt>indices_values = {0, 0, 1, 1, 3, 3}</tt>                  
/// @n  For axis: along_x:
/// @n      <tt>output_sizes  = (1, 2, 6, 2)</tt>
/// @n      <tt>output_values = (a, a, b, b, d, d)</tt>
/// @n      <tt>                (e, e, f, f, h, h)</tt>
/// @n
/// @n The resulting output will have sizes equal to input_size with changed concrete tensor size to inidices x size.
/// @n
/// @n@b Requirements:
/// @n - @c input must be a valid primitive_id, which output's format is bfyx/yxfb;
/// @n - @c indices must be a valid primitive_id, which output's layout is: (bfyx/yxfb, i32, {1, 1, indicies_size, 1})
/// @n - @c axis - valid index_select_axis_name instance. 
/// @n Breaking any of this conditions will cause exeption throw.
struct index_select : public primitive_base<index_select, CLDNN_PRIMITIVE_DESC(index_select)>
{
    CLDNN_DECLARE_PRIMITIVE(index_select)

    /// @brief Constructs index_select primitive / layer.
    ///
    /// @param id                 An identifier of new primitive.
    /// @param input              An identifier of primitive, which is an input for newly created
    ///                           index_select primitive.
    /// @param indicies           An identifer of primitive, which have indices in memory distributed along x. 
    /// @param axis               Axis of index selecting.
    /// @param output_padding     Optional padding for output from primitive.
    index_select(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& indices,
        index_select_axis_name axis = index_select_axis_name::along_b,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input, indices }, output_padding)
        , axis( { axis } )
        , reverse(false)
    {}

    /// @brief Constructs index_select primitive / layer.
    ///
    /// @param id                 An identifier of new primitive.
    /// @param input              An identifier of primitive, which is an input for newly created
    ///                           index_select primitive.
    /// @param axis               Axis of index selecting.
    /// @param output_padding     Optional padding for output from primitive.
    index_select(
        const primitive_id& id,
        const primitive_id& input,
        index_select_axis_name axis = index_select_axis_name::along_b,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input }, output_padding)
        , axis( { axis } )
        , reverse(true)
    {}

    /// @brief Constructs index_select primitive / layer.
    ///
    /// @param id                 An identifier of new primitive.
    /// @param input              An identifier of primitive, which is an input for newly created
    ///                           index_select primitive.
    /// @param axis               Vector of axes of index selecting.
    /// @param output_padding     Optional padding for output from primitive.
    index_select(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<index_select_axis_name>& axis = { index_select_axis_name::along_b },
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input }, output_padding)
        , axis(axis)
        , reverse(true)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{broadcast}
    index_select(const dto* dto)
        : primitive_base(dto)
        , axis(dto->axis, dto->axis + dto->axis_num)
        , reverse(dto->reverse)
    {}

    /// @brief A list of axes of index selecting
    std::vector<index_select_axis_name> axis;
    /// @brief Do index_select in reverse order on axis/axes.
    bool reverse;

protected:
    void update_dto(dto& dto) const override
    {
        dto.axis = axis.data();
        dto.axis_num = (int)axis.size();
        dto.reverse = reverse;
    }
};
/// @}
/// @}
/// @}
}
