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
#include "../C/tile.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs tile operation on input.
/// @details copies the input data n times across chosen axis.
struct tile : public primitive_base<tile, CLDNN_PRIMITIVE_DESC(tile)>
{
    CLDNN_DECLARE_PRIMITIVE(tile)

    enum tile_axis
    {
        along_b = cldnn_tile_along_b,
        along_f = cldnn_tile_along_f,
        along_x = cldnn_tile_along_x,
        along_y = cldnn_tile_along_y
    };

    /// @brief Constructs tile primitive.
    /// @param id This primitive id.
    /// @param axis Tiling axis
    /// @param tiles Tiles number across an axis
    tile(
        const primitive_id& id,
        const primitive_id& input,
        const tile_axis axis,
        const int tiles,
        const padding& output_padding = padding()
    )
        :primitive_base(id, {input}, output_padding)
        , axis(axis)
        , tiles(tiles)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{tile}
    tile(const dto* dto)
        :primitive_base(dto)
        , axis(static_cast<tile_axis>(dto->axis))
        , tiles(dto->tiles)
    {
    }

    /// @brief Tiling axis
    tile_axis axis;
    /// @brief Tiles number across an axis
    int tiles;
protected:

    void update_dto(dto& dto) const override
    {
        dto.axis = static_cast<cldnn_tile_axis>(axis);
        dto.tiles = tiles;
    }
};
/// @}
/// @}
/// @}
}
