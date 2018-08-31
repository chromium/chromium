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
#include "../C/convolution_grad_weights.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs backward convolution operation for weights and biases.
/// @details convolution_grad_weights updates weights and bias mutable data for training purposes.
/// @details Please note that this primitive was not heavily tested and currently only batch=1 is enabled for this primitive.
struct convolution_grad_weights : public primitive_base<convolution_grad_weights, CLDNN_PRIMITIVE_DESC(convolution_grad_weights)>
{
    CLDNN_DECLARE_PRIMITIVE(convolution_grad_weights)

    /// @brief Constructs convolution_grad_weights primitive.
    /// @param id This primitive id.
    /// @param input Input gradient primitive id.
    /// @param input Input primitive id from convolution forward pass.
    /// @param weights List of primitive ids containing weights data.
    /// @param bias List of primitive ids containing bias data. Provide empty vector if using next parameters without bias.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_weights window should start calculations.
    /// @param dilation Defines dilation size.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param conv_grad Id of primitive which uses weights and biases updated in this primitive. This is for correct order of calculating. Leave empty if primitive is last in backward pass.
    convolution_grad_weights(
        const primitive_id& id,
        const primitive_id& input_grad,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        const std::vector<primitive_id>& bias,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0, 0, 0, 0 },
        tensor dilation = { 1, 1, 1, 1 },
        const primitive_id& conv_grad = "",
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input_grad, input }, output_padding)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , prev_weights_grad(_prev_weights_grad.cpp_ids)
        , prev_bias_grad(_prev_bias_grad.cpp_ids)
        , conv_grad(conv_grad)
        , stride(stride)
        , input_offset(input_offset)
        , dilation(dilation)
        , _weights(weights)
        , _bias(bias)
        , _prev_weights_grad(std::vector<primitive_id>(0))
        , _prev_bias_grad(std::vector<primitive_id>(0))
    {
    }

    /// @brief Constructs convolution_grad_weights primitive (w/o bias).
    /// @param id This primitive id.
    /// @param input Input gradient primitive id.
    /// @param input Input primitive id from convolution forward pass.
    /// @param weights List of primitive ids containing weights data.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_weights window should start calculations.
    /// @param dilation Defines dilation size.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param conv_grad Id of primitive which uses weights and biases updated in this primitive. This is for correct order of calculating. Leave empty if primitive is last in backward pass.
    convolution_grad_weights(
        const primitive_id& id,
        const primitive_id& input_grad,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0, 0, 0, 0 },
        tensor dilation = { 1, 1, 1, 1 },
        const primitive_id& conv_grad = "",
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input_grad, input }, output_padding)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , prev_weights_grad(_prev_weights_grad.cpp_ids)
        , prev_bias_grad(_prev_bias_grad.cpp_ids)
        , conv_grad(conv_grad)
        , stride(stride)
        , input_offset(input_offset)
        , dilation(dilation)
        , _weights(weights)
        , _bias(std::vector<primitive_id>(0))
        , _prev_weights_grad(std::vector<primitive_id>(0))
        , _prev_bias_grad(std::vector<primitive_id>(0))
    {
    }

    /// @brief Constructs convolution_grad_weights primitive with momentum optimizer.
    /// @param id This primitive id.
    /// @param input Input gradient primitive id.
    /// @param input Input primitive id from convolution forward pass.
    /// @param weights List of primitive ids containing weights data.
    /// @param bias List of primitive ids containing bias data. Provide empty vector if using next parameters without bias.
    /// @param prev_weights_grad List of primitive ids which contains weights gradient data calculated in previous iteration. Used in momentum optimizer.
    /// @param prev_bias_grad List of primitive ids which contains bias gradient data calculated in previous iteration. Used in momentum optimizer.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_weights window should start calculations.
    /// @param dilation Defines dilation size.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param conv_grad Id of primitive which uses weights and biases updated in this primitive. This is for correct order of calculating. Leave empty if primitive is last in backward pass.
    convolution_grad_weights(
        const primitive_id& id,
        const primitive_id& input_grad,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        const std::vector<primitive_id>& bias,
        const std::vector<primitive_id>& prev_weights_grad,
        const std::vector<primitive_id>& prev_bias_grad,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0, 0, 0, 0 },
        tensor dilation = { 1, 1, 1, 1 },
        const primitive_id& conv_grad = "",
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input_grad, input }, output_padding)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , prev_weights_grad(_prev_weights_grad.cpp_ids)
        , prev_bias_grad(_prev_bias_grad.cpp_ids)
        , conv_grad(conv_grad)
        , stride(stride)
        , input_offset(input_offset)
        , dilation(dilation)
        , _weights(weights)
        , _bias(bias)
        , _prev_weights_grad(prev_weights_grad)
        , _prev_bias_grad(prev_bias_grad)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{convolution_grad_weights}
    convolution_grad_weights(const dto* dto)
        :primitive_base(dto)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , prev_weights_grad(_prev_weights_grad.cpp_ids)
        , prev_bias_grad(_prev_bias_grad.cpp_ids)
        , conv_grad(dto->conv_grad)
        , stride(dto->stride)
        , input_offset(dto->input_offset)
        , dilation(dto->dilation)
        , _weights(dto->weights)
        , _bias(dto->bias)
        , _prev_weights_grad(dto->prev_weights_grad)
        , _prev_bias_grad(dto->prev_bias_grad)
    {
        if (!dto->split || (weights.size() != bias.size() && bias.size() != 0) || dto->split != weights.size())
            throw std::invalid_argument("Invalid convolution_grad_weights dto: bad split value");
    }

    /// @brief List of primitive ids containing weights data.
    fixed_size_vector_ref weights;
    /// @brief List of primitive ids containing bias data.
    fixed_size_vector_ref bias;
    /// @brief Array of primitive ids containing weights gradient data calculated in previous iteration. Amount of primitives and their memory sizes should be same as weights.
    fixed_size_vector_ref prev_weights_grad;
    /// @brief Array of primitive ids containing bias gradient data calculated in previous iteration. Amount of primitives and their memory sizes should be same as biases.
    fixed_size_vector_ref prev_bias_grad;
    /// @brief Primitive id containing convolution gradient data.
    primitive_id conv_grad;
    /// @brief Defines shift in input buffer between adjacent calculations of output values.
    tensor stride;
    /// @brief Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_weights window should start calculations.
    tensor input_offset;
    /// @brief Defines gaps in the input - dilation rate k=1 is normal convolution, k=2 means skipping one pixel per input, k=4 means skipping 3 pixels.
    /// As an example in one dimension, a filter w of size 3 would compute over input x the following: w[0]*x[0] + w[1]*x[1] + w[2]*x[2] for dilation of 1. 
    /// For dilation 2 the filter would instead compute w[0]*x[0] + w[1]*x[2] + w[2]*x[4].
    tensor dilation;

    /// @brief On how many cards split the computation to.
    int32_t split() const { return static_cast<int32_t>(weights.size()); }

protected:
    primitive_id_arr _weights;
    primitive_id_arr _bias;
    primitive_id_arr _prev_weights_grad;
    primitive_id_arr _prev_bias_grad;

    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        ret.reserve(weights.size() + bias.size() + !conv_grad.empty() + prev_weights_grad.size() + prev_bias_grad.size());
        for (auto& w : weights)
            ret.push_back(w);
        for (auto& b : bias)
            ret.push_back(b);

        for (auto& g : prev_weights_grad)
            ret.push_back(g);
        for (auto& g : prev_bias_grad)
            ret.push_back(g);
        if (!conv_grad.empty())
            ret.push_back(conv_grad);

        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.weights = _weights.ref();
        dto.bias = _bias.ref();
        dto.input_offset = input_offset;
        dto.dilation = dilation;
        dto.split = split();
        dto.stride = stride;
        dto.conv_grad = conv_grad.c_str();
        dto.prev_bias_grad = _prev_bias_grad.ref();
        dto.prev_weights_grad = _prev_weights_grad.ref();
    }
};
/// @}
/// @}
/// @}
}