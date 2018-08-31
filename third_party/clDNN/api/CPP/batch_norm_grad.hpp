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
#include "../C/batch_norm_grad.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs backward batch normalization layer.
/// @details Calculates mean gradient and gradient * input for every feature in data, 
/// then output is calculated as inv_variance * (input_grad - mean_grad_input * input - mean_grad)
struct batch_norm_grad : public primitive_base<batch_norm_grad, CLDNN_PRIMITIVE_DESC(batch_norm_grad)>
{
    CLDNN_DECLARE_PRIMITIVE(batch_norm_grad)

    /// @brief Constructs batch normalization backward layer.
    /// @param id This primitive id.
    /// @param input_grad Input gradient primitive id.
    /// @param input Input primitive id.
    /// @param inv_variance Primitive id containing inverted variance from forward pass.
    batch_norm_grad(
        const primitive_id& id,
        const primitive_id& input_grad,
        const primitive_id& input,
        const primitive_id& inv_variance,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input_grad, input }, output_padding)
        , inv_variance(inv_variance)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{batch_norm_grad}
    batch_norm_grad(const dto* dto)
        :primitive_base(dto)
        , inv_variance(dto->inv_variance)
    {
    }

    /// @brief Primitive id containing inverted variance from forward pass.
    primitive_id inv_variance;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        return{ inv_variance };
    }

    void update_dto(dto& dto) const override
    {
        dto.inv_variance = inv_variance.c_str();
    }
};
/// @}
/// @}
/// @}
}