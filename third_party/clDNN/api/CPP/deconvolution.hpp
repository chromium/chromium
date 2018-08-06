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
#include "../C/deconvolution.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs transposed convolution.
/// Also supports built-in Relu @ref activation available by setting it in arguments.
/// @details Deconvolution is similar to convolution layer with the weights flipped on the axis and stride and input padding parameters used in opposite sense as in convolution.
struct deconvolution : public primitive_base<deconvolution, CLDNN_PRIMITIVE_DESC(deconvolution)>
{
    CLDNN_DECLARE_PRIMITIVE(deconvolution)

    /// @brief Constructs deconvolution primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param bias List of primitive ids containing bias data. Provide empty vector if using next parameters without bias.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the deconvolution window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    deconvolution(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        const std::vector<primitive_id>& bias,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0,0,0,0 },
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input }, output_padding)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , input_offset(input_offset)
        , stride(stride)
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , with_output_size(false)
        , _weights(weights)
        , _bias(bias)
        , _gradient(false)
    {
    }

    /// @brief Constructs deconvolution primitive (w/o bias).
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the deconvolution window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    deconvolution(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0,0,0,0 },
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding(),
        bool gradient = false
    )
        :primitive_base(id, { input }, output_padding)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , input_offset(input_offset)
        , stride(stride)
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , with_output_size(false)
        , _weights(weights)
        , _bias(std::vector<primitive_id>(0))
        , _gradient(gradient)
    {
    }

    /// @brief Constructs deconvolution primitive (computes input paddings to match output size).
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param bias List of primitive ids containing bias data. Provide empty vector if using next parameters without bias.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the deconvolution window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    /// @param output_size User-defined output data size of the primitive (w/o padding).
    deconvolution(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        const std::vector<primitive_id>& bias,
        tensor stride,
        tensor input_offset,
        bool with_activation,
        float activation_slp,
        tensor output_size,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input }, output_padding)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , input_offset(input_offset)
        , stride(stride)
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , with_output_size(true)
        , output_size(output_size)
        , _weights(weights)
        , _bias(bias)
        , _gradient(false)
    {
    }

    /// @brief Constructs deconvolution primitive (w/o bias, computes input paddings to match output size).
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the deconvolution window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    /// @param output_size User-defined output data size of the primitive (w/o padding).
    deconvolution(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        tensor stride,
        tensor input_offset,
        bool with_activation,
        float activation_slp,
        tensor output_size,
        const padding& output_padding = padding(),
        bool gradient = false
    )
        :primitive_base(id, { input }, output_padding)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , input_offset(input_offset)
        , stride(stride)
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , with_output_size(true)
        , output_size(output_size)
        , _weights(weights)
        , _bias(std::vector<primitive_id>(0))
        , _gradient(gradient)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{deconvolution}
    deconvolution(const dto* dto)
        :primitive_base(dto)
        , weights(_weights.cpp_ids)
        , bias(_bias.cpp_ids)
        , input_offset(dto->input_offset)
        , stride(dto->stride)
        , with_activation(dto->with_activation != 0)
        , activation_negative_slope(dto->activation_negative_slope)
        , with_output_size(dto->with_output_size != 0)
        , output_size(dto->output_size)
        , _weights(dto->weights)
        , _bias(dto->bias)
        , _gradient(dto->gradient != 0)
    {
        if (!dto->split || (weights.size() != bias.size() && bias.size() != 0) || dto->split != weights.size())
            throw std::invalid_argument("Invalid deconvolution dto: bad split value");
    }

    /// @brief Constructs deconvolution primitive (computes input paddings to match output size).
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param bias List of primitive ids containing bias data. Provide empty vector if using next parameters without bias.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the deconvolution window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    /// @param output_size User-defined output data size of the primitive (w/o padding).
    /// @return Deconvolution primitive with specified settings.
    static deconvolution create_with_output_size(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        const std::vector<primitive_id>& bias,
        tensor output_size,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0,0,0,0 },
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
    {
        return deconvolution(id, input, weights, bias, stride, input_offset, with_activation,
            activation_slp, output_size, output_padding);
    }

    /// @brief Constructs deconvolution primitive (w/o bias; computes input paddings to match output size).
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the deconvolution window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    /// @param output_size User-defined output data size of the primitive (w/o padding).
    /// @return Deconvolution primitive with specified settings.
    static deconvolution create_with_output_size(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        tensor output_size,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0,0,0,0 },
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
    {
        return deconvolution(id, input, weights, stride, input_offset, with_activation,
            activation_slp, output_size, output_padding);
    }

    /// @brief List of primitive ids containing weights data.
    fixed_size_vector_ref weights;
    /// @brief List of primitive ids containing bias data.
    fixed_size_vector_ref bias;
    /// @brief Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the deconvolution window should start calculations.
    tensor input_offset;
    /// @brief Defines shift in input buffer between adjacent calculations of output values.
    tensor stride;
    /// @brief Enables Relu activation.
    bool with_activation;
    /// @brief Relu activation slope.
    float activation_negative_slope;
    /// @brief Indicates that the primitive has user-defined output size (non-zero value).
    bool with_output_size;
    /// @brief User-defined output data size of the primitive (w/o padding).
    tensor output_size;

    /// @brief On how many cards split the computation to.
    int32_t split() const { return static_cast<int32_t>(weights.size()); }
    /// @brief Indicates that deconvolution is used for convolution backward computation (convolution_grad_input)
    bool gradient() const { return _gradient; }

protected:
    primitive_id_arr _weights;
    primitive_id_arr _bias;
    bool _gradient;

    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        ret.reserve(weights.size() + bias.size());
        for (auto& w : weights)
            ret.push_back(w);
        for (auto& b : bias)
            ret.push_back(b);

        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.weights = _weights.ref();
        dto.bias = _bias.ref();
        dto.input_offset = input_offset;
        dto.split = split();
        dto.stride = stride;
        dto.with_activation = with_activation;
        dto.activation_negative_slope = activation_negative_slope;
        dto.with_output_size = with_output_size;
        dto.output_size = output_size;
        dto.gradient = _gradient;
    }
};
/// @}
/// @}
/// @}
}