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

#include "../C/border.h"
#include "primitive.hpp"


namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Type of border that will be added to the input by border layer / primitive.
enum class border_type : std::int32_t
{
    /// @brief All points in the border are set to constant value.
    constant = cldnn_border_constant,
    zero = cldnn_border_zero,
    /// @brief Border is constructed as an mirror of image (edge is also mirrored).
    /// @details Size of border in any dimension cannot be larger than size of
    ///          input in the same dimension.
    mirror = cldnn_border_mirror,
    /// @brief Border is constructed as an mirror of image (edge is NOT mirrored).
    /// @details Size of border in any dimension cannot be larger than size of
    ///          input in the same dimension decreased by @c 1.
    mirror_101 = cldnn_border_mirror_101,
    /// @brief Border is constructed as an replication of edge.
    /// @details Size of border in any dimension cannot be larger than size of
    ///          input in the same dimension.
    edge = cldnn_border_edge
};


/// @brief Adds border around input.
///
/// @details Applies border of specified type around input data. The size of output data is increased
///          by @c left_top_sizes and by @right_bottom_sizes.
/// @n
/// @n@b Requirements:
/// @n - @c left_top_sizes and @c right_bottom_sizes must be non-negative on all dimensions and compatible
///      with size of input (describe the same dimensions).
/// @n - For @c border_type equal to @c cldnn_border_mirror, @c left_top_sizes and @c right_bottom_sizes
///      must be lower than or equal to size of input on corresponding dimension (for all dimensions)
/// @n - For @c border_type equal to @c cldnn_border_mirror_101, @c left_top_sizes and @c right_bottom_sizes
///      must be lower than size of input on corresponding dimension (for all dimensions)
/// @n Breaking any of this conditions will cause exeption throw.
struct border : public primitive_base<border, CLDNN_PRIMITIVE_DESC(border)>
{
    CLDNN_DECLARE_PRIMITIVE(border)

    /// @brief Constructs border primitive / layer.
    ///
    /// @param id                 An identifier of new primitive.
    /// @param input              An identifier of primitive which is an input for newly created
    ///                           border primitive.
    /// @param left_top_sizes     Sizes of border that needs to be added from left
    ///                           (in X dimension) and from top (in Y dimension).
    /// @param right_bottom_sizes Sizes of border that needs to be added from right
    ///                           (in X dimension) and from bottom (in Y dimension).
    /// @param type               Type of added border.
    /// @param border_value       Value of elements which is used for paddings
    /// @param output_padding     Optional padding for output from primitive.
    border(
        const primitive_id& id,
        const primitive_id& input,
        const tensor& left_top_sizes = { 0, 0, 0, 0 },
        const tensor& right_bottom_sizes = { 0, 0, 0, 0 },
        const border_type type = border_type::constant,
        const float border_value = 0.0f,
        const padding& output_padding = padding()
    )
        : primitive_base(id, {input}, output_padding),
          left_top_sizes(left_top_sizes),
          right_bottom_sizes(right_bottom_sizes),
          type(type),
          border_value(border_value)
    {
    }

    /// @brief Constructs border primitive / layer.
    ///
    /// @param id                 An identifier of new primitive.
    /// @param input              An identifier of primitive which is an input for newly created
    ///                           border primitive.
    /// @param x_y_sizes          Sizes of border that needs to be added from left and right
    ///                           (in X dimension) and from top and bottom (in Y dimension).
    ///                           Created border is simmetric (the same size of border applied
    ///                           from both sides of input).
    /// @param type               Type of added border.
    /// @param output_padding     Optional padding for output from primitive.
    border(
        const primitive_id& id,
        const primitive_id& input,
        const tensor& x_y_sizes,
        const border_type type = border_type::constant,
        const padding& output_padding = padding()
    )
        : border(id, input, x_y_sizes, x_y_sizes, type, 0.0f, output_padding)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{border}
    border(const dto* dto)
        : primitive_base(dto),
          left_top_sizes(dto->left_top_sizes),
          right_bottom_sizes(dto->right_bottom_sizes),
          type(static_cast<border_type>(dto->border_type)),
          border_value(dto->border_value)
    {
    }

    /// @brief Sizes of border that needs to be added from left (in X dimension) and from top (in Y dimension).
    tensor left_top_sizes;
    /// @brief Sizes of border that needs to be added from right (in X dimension) and from bottom (in Y dimension).
    tensor right_bottom_sizes;
    /// @brief Type of border that needs to be added to the input.
    border_type type;
    /// @brief Border value that is used in constant mode.
    float border_value;
protected:
    void update_dto(dto& dto) const override
    {
        dto.left_top_sizes     = left_top_sizes;
        dto.right_bottom_sizes = right_bottom_sizes;
        dto.border_type        = static_cast<cldnn_border_type>(type);
        dto.border_value       = border_value;
    }
};
/// @}
/// @}
/// @}
}
