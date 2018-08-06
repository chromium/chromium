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
#include "../C/activation_grad.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Activation gradient for rectified linear unit or parameterized rectified linear unit.
/// @par Algorithm:
///   out(i,x,y) = input_gradient(i,x,y) * ((input(i,x,y) > 0) + slope(i)  * (input(i,x,y) <= 0)
/// @par Where:
///   @li out(i,x,y) : value at x, y from i-th feature map after activation.
///   @li in(i,x,y) : value at x, y from i-th feature map before activation.
///   @li slope(i) : the slope value of the i-th feature map (can be shared across channels or one slope per channel).
struct activation_grad : public primitive_base<activation_grad, CLDNN_PRIMITIVE_DESC(activation_grad)>
{
    CLDNN_DECLARE_PRIMITIVE(activation_grad)

    /// @brief Constructs Relu grad primitive.
    /// @param id This primitive id.
    /// @param input_grad Input gradient primitive id.
    /// @param input Input primitive id.
    /// @param activation_grad_func activation_grad function.
    /// @param additional_params additional params (slope).
    activation_grad(
        const primitive_id& id,
        const primitive_id& input_grad,
        const primitive_id& input,
        cldnn_activation_grad_func activation_grad_func,
        cldnn_activation_additional_params additional_params = { 0.f,0.f },
        const padding& output_padding = padding()
        )
        : primitive_base(id, { input_grad, input }, output_padding)
        , activation_grad_func(activation_grad_func)
        , additional_params(additional_params)
        , additional_params_input("")
    {
    }

    /// @brief Constructs Relu grad primitive.
    /// @param id This primitive id.
    /// @param input_grad Input gradient primitive id.
    /// @param input Input primitive id.
    /// @param activation_grad_func activation_grad function.
    /// @param additional_params additional params (slope).
    activation_grad(
        const primitive_id& id,
        const primitive_id& input_grad,
        const primitive_id& input,
        const primitive_id& additional_params_input,
        cldnn_activation_grad_func activation_grad_func,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input_grad, input }, output_padding)
        , activation_grad_func(activation_grad_func)
        , additional_params({ 0,0 })
        , additional_params_input(additional_params_input)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{activation_grad}
    activation_grad(const dto* dto)
        : primitive_base(dto)
        , activation_grad_func(dto->activation_grad_func)
        , additional_params(dto->additional_params)
        , additional_params_input(dto->additional_params_input)
    {
    }

    /// @brief activation_grad function.
    cldnn_activation_grad_func activation_grad_func;

    /// @brief activation_grad additional params.
    cldnn_activation_additional_params additional_params;

    /// @brief PRelu activation slope input primitive id.
    /// Input x dimension should be equal to input feature size (one slope per channel).
    /// All other dimensions should be 1.
    primitive_id additional_params_input;

protected:

    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        if (additional_params_input.empty())
            return{};
        return{ additional_params_input };
    }

    void update_dto(dto& dto) const override
    {
        dto.activation_grad_func = activation_grad_func;
        dto.additional_params = additional_params;
        dto.additional_params_input = additional_params_input.c_str();
    }
};
/// @}
/// @}
/// @}
}