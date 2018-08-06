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
#include "../C/permute.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Permutes data in the memory, with respect to provided order.
/// @details Permute order is set as vector with positions meaning corresponding to tensor.
/// Vector values represent dimensions to be permuted in bfyx format. For example: <br>
/// input_dimensions = tensor{ 5, 3, 6, 3 } <br>
/// permute_order = { 2, 3, 1, 0 } <br>
/// output_dimensions = { 6, 3, 3, 5 } <br>
/// <br>
/// When permute_order is { 0, 1, 2, 3 } then input_dimensions = output_dimensions
struct permute : public primitive_base<permute, CLDNN_PRIMITIVE_DESC(permute)>
{
    CLDNN_DECLARE_PRIMITIVE(permute)

    /// @brief Constructs permute primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param permute_order Array of permuted output order in bfyx format.
    permute(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<uint16_t>& permute_order = {},
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input }, output_padding)
        , permute_order(permute_order)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{reorder}
    permute(const dto* dto)
        : primitive_base(dto)
        , permute_order(uint16_t_arr_to_vector(dto->permute_order))
    {
    }

    /// @brief Array of permuted output order in bfyx format.
    std::vector<uint16_t> permute_order;

protected:
    void update_dto(dto& dto) const override
    {
        dto.permute_order = uint16_t_vector_to_arr(permute_order);
    }
};
/// @}
/// @}
/// @}
}
