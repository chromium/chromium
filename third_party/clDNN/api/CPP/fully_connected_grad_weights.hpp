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
#include "../C/fully_connected_grad_weights.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs backward fully connected layer (inner product) for weights and biases.

struct fully_connected_grad_weights : public primitive_base<fully_connected_grad_weights, CLDNN_PRIMITIVE_DESC(fully_connected_grad_weights)>
{
    CLDNN_DECLARE_PRIMITIVE(fully_connected_grad_weights)

    /// @brief Constructs fully connected layer for weights and biases.
    /// @param id This primitive id.
    /// @param input Input gradient primitive id.
    /// @param input Input primitive id.
    /// @param weights Primitive id containing weights data.
    /// @param bias Primitive id containing bias data. Provide empty string if using Relu without bias.
    /// @param fc_grad Id of primitive which uses weights and biases updated in this primitive. This is for correct order of calculating. Leave empty if primitive is last in backward pass.
    fully_connected_grad_weights(
        const primitive_id& id,
        const primitive_id& input_grad,
        const primitive_id& input,
        const primitive_id& weights,
        const primitive_id& bias = "",
        const primitive_id& fc_grad = "",
        const padding& output_padding = padding()
        )
        : primitive_base(id, { input_grad, input }, output_padding)
        , weights(weights)
        , bias(bias)
        , fc_grad(fc_grad)
        , prev_weights_grad("")
        , prev_bias_grad("")
    {
    }

    /// @brief Constructs fully connected layer for weights and biases with momentum optimizer.
    /// @param id This primitive id.
    /// @param input Input gradient primitive id.
    /// @param input Input primitive id.
    /// @param weights Primitive id containing weights data.
    /// @param bias Primitive id containing bias data. Provide empty string if using Relu without bias.
    /// @param prev_weights_grad Id of primitive which contains weights gradient data calculated in previous iteration. Used in momentum optimizer.
    /// @param prev_bias_grad Id of primitive which contains bias gradient data calculated in previous iteration. Used in momentum optimizer.
    /// @param fc_grad Id of primitive which uses weights and biases updated in this primitive. This is for correct order of calculating.
    fully_connected_grad_weights(
        const primitive_id& id,
        const primitive_id& input_grad,
        const primitive_id& input,
        const primitive_id& weights,
        const primitive_id& bias,
        const primitive_id& prev_weights_grad,
        const primitive_id& prev_bias_grad,
        const primitive_id& fc_grad = "",
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input_grad, input }, output_padding)
        , weights(weights)
        , bias(bias)
        , fc_grad(fc_grad)
        , prev_weights_grad(prev_weights_grad)
        , prev_bias_grad(prev_bias_grad)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{fully_connected_grad_weights}
    fully_connected_grad_weights(const dto* dto)
        :primitive_base(dto)
        , weights(dto->weights)
        , bias(dto->bias)
        , fc_grad(dto->fc_grad)
        , prev_weights_grad(dto->prev_weights_grad)
        , prev_bias_grad(dto->prev_bias_grad)
    {
    }

    /// @brief Primitive id containing weights data.
    primitive_id weights;
    /// @brief Primitive id containing bias data.
    primitive_id bias;
    /// @brief Primitive id containing fully connected gradient data.
    primitive_id fc_grad;
    /// @brief Id of primitive containing weights gradient data calculated in previous iteration. It's memory size should be same as weights.
    primitive_id prev_weights_grad;
    /// @brief Id of primitive containing bias gradient data calculated in previous iteration. It's memory size should be same as biases.
    primitive_id prev_bias_grad;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override 
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        ret.reserve(1 + !bias.empty() + !fc_grad.empty() + !prev_weights_grad.empty() + !prev_bias_grad.empty());

        ret.push_back(weights);
        if (!bias.empty())
            ret.push_back(bias);

        if (!prev_weights_grad.empty())
            ret.push_back(prev_weights_grad);
        if (!prev_bias_grad.empty())
            ret.push_back(prev_bias_grad);
        if (!fc_grad.empty())
            ret.push_back(fc_grad);

        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.weights = weights.c_str();
        dto.bias = bias.c_str();
        dto.fc_grad = fc_grad.c_str();
        dto.prev_weights_grad = prev_weights_grad.c_str();
        dto.prev_bias_grad = prev_bias_grad.c_str();
    }
};
/// @}
/// @}
/// @}
}