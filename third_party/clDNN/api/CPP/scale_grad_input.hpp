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
#include "../C/scale_grad_input.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs scale primitive backward for input.
struct scale_grad_input : public primitive_base<scale_grad_input, CLDNN_PRIMITIVE_DESC(scale_grad_input)>
{
    CLDNN_DECLARE_PRIMITIVE(scale_grad_input)

    /// @brief Constructs scale_grad_input.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param scale_input Scale input primitive id with values needed for product computation.
    scale_grad_input(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& scale_input, //should be bfyx or yxfb, where each dimension can be 1, if all dimensions are 1 then this is scalar
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input, scale_input }, output_padding)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{scale_grad_input}
    scale_grad_input(const dto* dto)
        :primitive_base(dto)
    {
        if (dto->input.size != 2)
            throw std::invalid_argument("scale_grad_input dto should contains exactly 2 inputs");
    }

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        return{};
    }

    void update_dto(dto&) const override {}
};
/// @}
/// @}
/// @}
}
