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
#include "../C/crop.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{


/// @brief Marker type indicating that instead of reference input size left, top,
///        right and bottom borders (to cut out) should be specified.
///
/// @details Used to differentiate constructors.
struct crop_borders_t {};

/// @brief Marker indicating that instead of reference input size left, top,
///        right and bottom borders (to cut out) should be specified.
constexpr auto crop_borders = crop_borders_t{};

/// @brief Performs crop operation on input.
/// @details Crops the input to the shape of reference_input across all dimensions taking into account specified input offsets.
/// @n       Borders variant calculated output shape from input shape minus the specified borders.
/// @n
/// @n\b Examples
/// @n Crop without offset example:
/// \image html crop_no_offset.jpg
/// @n Crop with offset example:
/// \image html crop_w_offset.jpg
/// @n
/// @n\b Requirements (reference size variant)
/// @n - Input size cannot be greater than reference size in any dimension
/// @n - All sizes have to have positive numbers
/// @n - Reference size plus offset cannot exceed input size
/// @n
/// @n\b Requirements (borders variant)
/// @n - Borders support batch, feature and spatial dimensions (rest of dimensions ignored).
/// @n - Input size cannot be greater than reference size in any dimension
/// @n - All sizes specified in borders have to have non-negative values (positive or @c 0).
/// @n - Sum of sizes of opposite borders must be lower than input size (on all non-ignored dimensions).
/// @n
/// @n Breaking any of this conditions will cause exception throw.
struct crop : public primitive_base<crop, CLDNN_PRIMITIVE_DESC(crop)>
{
    CLDNN_DECLARE_PRIMITIVE(crop)

    /// @brief Constructs crop primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param reference_input Reference input tensor with the required dimensions.
    /// @param offsets Input offsets.
    crop(
        const primitive_id& id,
        const primitive_id& input,
        const tensor& reference_input,
        const tensor& offsets,
        const padding& output_padding = padding()
    )
        :primitive_base(id, {input}, output_padding)
        , reference_input(reference_input)
        , offsets(offsets)
    {
    }

    /// @brief Constructs crop primitive (borders variant).
    ///
    /// @details Allows to specify borders from each side that should be cut out
    ///          by the primitive.
    /// @n       NOTE: Borders variant supports only up to four dimensions.
    ///
    /// @param id         Identifier of newly created primitive.
    /// @param input      Identifier of input primitive which dimensions will be cropped.
    /// @param lt_borders Border sizes (spatial dimensions define left (X) and top (Y)
    ///                   borders, non-spatial dimensions - lower borders)
    /// @param rb_borders Border sizes (spatial dimensions define right (X) and bottom (Y)
    ///                   borders, non-spatial dimensions - upper borders)
    crop(
        const primitive_id& id,
        const primitive_id& input,
        const tensor& lt_borders,
        const tensor& rb_borders,
        const crop_borders_t,
        const padding& output_padding = padding()
    )
        :primitive_base(id, {input}, output_padding)
        , reference_input(rb_borders.negate())
        , offsets(lt_borders)
    {
    }

    /// @brief Constructs crop primitive (symmetric borders variant).
    ///
    /// @details Allows to specify borders from each side that should be cut out
    ///          by the primitive.
    /// @n       NOTE: Borders variant supports only up to four dimensions.
    ///
    /// @param id         Identifier of newly created primitive.
    /// @param input      Identifier of input primitive which dimensions will be cropped.
    /// @param xy_borders Border sizes (symmetric; spatial dimensions define left/right (X)
    ///                   and top/bottom (Y) borders, non-spatial dimensions - lower/upper borders).
    crop(
        const primitive_id& id,
        const primitive_id& input,
        const tensor& xy_borders,
        const crop_borders_t,
        const padding& output_padding = padding()
    )
        :primitive_base(id, {input}, output_padding)
        , reference_input(xy_borders.negate())
        , offsets(xy_borders)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{crop}
    crop(const dto* dto)
        :primitive_base(dto)
        , reference_input(dto->reference_input)
        , offsets(dto->offsets)
    {
    }

    /// @brief Reference input tensor with the required dimensions.
    tensor reference_input;
    /// @brief Input offsets.
    tensor offsets;

protected:
    void update_dto(dto& dto) const override
    {
        dto.reference_input = reference_input;
        dto.offsets = offsets;
    }
};
/// @}
/// @}
/// @}
}
