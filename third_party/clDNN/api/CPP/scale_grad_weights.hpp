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
#include "../C/scale_grad_weights.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs scale layer backward for scale_input and biases. 
struct scale_grad_weights : public primitive_base<scale_grad_weights, CLDNN_PRIMITIVE_DESC(scale_grad_weights)>
{
    CLDNN_DECLARE_PRIMITIVE(scale_grad_weights)

    /// @brief Constructs scale_grad_weights primitive without bias.
    /// @param id This primitive id.
    /// @param input Input primitive id. Same as input for scale forward.
    /// @param input_grad Input gradient primitive id.
    /// @param scale_input Scale input primitive id.
    /// @param scale_grad Id of primitive which uses weights and biases updated in this primitive. This is for correct order of calculating.
    scale_grad_weights(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& input_grad,
        const primitive_id& scale_input, //should be one number per feature
        const primitive_id& scale_grad = "", //leave empty if this is last primitive in backward pass
        const padding& output_padding = padding()
     )
        :primitive_base(id, { input, input_grad }, output_padding)
        , scale_input(scale_input)
        , bias("")
        , prev_scale_grad("")
        , prev_bias_grad("")
        , scale_grad(scale_grad)
    {
    }

    /// @brief Constructs scale_grad_weights primitive with optional adding bias.
    /// @param id This primitive id.
    /// @param input Input primitive id. Same as input for scale forward.
    /// @param input_grad Input gradient primitive id.
    /// @param scale_input Scale input primitive id.
    /// @param bias Primitive id containing bias data.
    /// @param scale_grad Id of primitive which uses weights and biases updated in this primitive. This is for correct order of calculating.
    scale_grad_weights(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& input_grad,
        const primitive_id& scale_input, //should be one number per feature
        const primitive_id& bias, //should be same size as scale_input
        const primitive_id& scale_grad = "", //leave empty if this is last primitive in backward pass
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input,  input_grad }, output_padding)
        , scale_input(scale_input)
        , bias(bias)
        , prev_scale_grad("")
        , prev_bias_grad("")
        , scale_grad(scale_grad)
    {
    }

    /// @brief Constructs scale_grad_weights primitive with optional bias and momentum optimizer.
    /// @param id This primitive id.
    /// @param input Input primitive id. Same as input for scale forward.
    /// @param input_grad Input gradient primitive id.
    /// @param scale_input Scale input primitive id.
    /// @param bias Primitive id containing bias data.
    /// @param prev_scale_grad Id of primitive which contains scale gradient data calculated in previous iteration. Used in momentum optimizer.
    /// @param prev_bias_grad Id of primitive which contains bias gradient data calculated in previous iteration. Used in momentum optimizer.
    /// @param scale_grad Id of primitive which uses weights and biases updated in this primitive. This is for correct order of calculating.
    scale_grad_weights(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& input_grad,
        const primitive_id& scale_input, //should be one number per feature
        const primitive_id& bias, //should be same size as scale_input
        const primitive_id& prev_scale_grad,
        const primitive_id& prev_bias_grad, //leave empty if bias not specified
        const primitive_id& scale_grad = "", //leave empty if this is last primitive in backward pass
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input,  input_grad }, output_padding)
        , scale_input(scale_input)
        , bias(bias)
        , prev_scale_grad(prev_scale_grad)
        , prev_bias_grad(prev_bias_grad)
        , scale_grad(scale_grad)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{scale_grad_weights}
    scale_grad_weights(const dto* dto)
        :primitive_base(dto)
        , scale_input(dto->scale_input)
        , bias(dto->bias)
        , prev_scale_grad(dto->prev_scale_grad)
        , prev_bias_grad(dto->prev_bias_grad)
        , scale_grad(dto->scale_grad)
    {
    }

    /// @brief Scale input primitive id.
    primitive_id scale_input;
    /// @brief Primitive id containing bias data.
    primitive_id bias;
    /// @brief Primitive id containing scale gradient data calculated in previous iteration.
    primitive_id prev_scale_grad;
    /// @brief Primitive id containing bias gradient data calculated in previous iteration. 
    primitive_id prev_bias_grad;
    /// @brief Primitive id which uses weights and biases updated in this primitive.
    primitive_id scale_grad;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        ret.reserve(1 + !bias.empty() + !prev_scale_grad.empty() + !prev_bias_grad.empty());

        ret.push_back(scale_input);
        if (!bias.empty())
            ret.push_back(bias);
        if (!prev_scale_grad.empty())
            ret.push_back(prev_scale_grad);
        if (!prev_bias_grad.empty())
            ret.push_back(prev_bias_grad);
        if (!scale_grad.empty())
            ret.push_back(scale_grad);

        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.bias = bias.c_str();
        dto.scale_input = scale_input.c_str();
        dto.prev_scale_grad = prev_scale_grad.c_str();
        dto.prev_bias_grad = prev_bias_grad.c_str();
        dto.scale_grad = scale_grad.c_str();
    }
};
/// @}
/// @}
/// @}
}
