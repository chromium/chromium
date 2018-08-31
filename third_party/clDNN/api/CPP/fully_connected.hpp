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
#include "../C/fully_connected.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs forward fully connected layer (inner product).
/// Also supports built-in Relu @CLDNN_PRIMITIVE_DESC{activation} available by setting it in arguments.
/// @notes 
/// - Equation: Input[F x Y x F] x Output(X) == Weights(B x F x X x F) has to be fulfilled
/// - Bias has to be linear data [1,1,1,X], where X is equal to number of outputs.

/// <table>
/// <caption id = "multi_row">Format support</caption>
///        <tr><th>Data type               <th>activation format       <th>weights format
///        <tr><td rowspan="7">F32         <td rowspan="4">bfyx        <td>yxfb
///        <tr>                                                        <td>fyxb
///        <tr>                                                        <td>bs_xs_xsv8_bsv8
///        <tr>                                                        <td>bs_x_bsv16
///        <tr>                            <td rowspan="3">yxfb        <td>bfyx
///        <tr>                                                        <td>yxfb
///        <tr>                                                        <td>bs_xs_xsv8_bsv8
///        <tr><td rowspan="4">F16         <td rowspan="3">bfyx        <td>yxfb
///        <tr>                                                        <td>fyxb
///        <tr>                                                        <td>bs_x_bsv16
///        <tr>                            <td >yxfb                   <td>bfyx
/// </table>

struct fully_connected : public primitive_base<fully_connected, CLDNN_PRIMITIVE_DESC(fully_connected)>
{
    CLDNN_DECLARE_PRIMITIVE(fully_connected)

    /// @brief Constructs fully connected layer.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights Primitive id containing weights data.
    /// @param bias Primitive id containing bias data. Provide empty string if using Relu without bias.
    /// @param with_activation Enable Relu activation.
    /// @param activation_slp Relu activation slope.
    fully_connected(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& weights,
        const primitive_id& bias = "",
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
        )
        : primitive_base(id, {input}, output_padding)
        , weights(weights)
        , bias(bias)
        , weights_quantization_factors("")
        , output_calibration_factors("")
        , input_quantization_factor(1.0f)
        , output_quantization_factor(1.0f)
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
    {
    }

    /// @brief Constructs fully connected layer.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights Primitive id containing weights data.
    /// @param bias Primitive id containing bias data. Provide empty string if using Relu without bias.
    /// @param w_quantization_factor Primitive id containing weights quanitization factors per output feature map.
    /// @param i_quantization_factor Input quantization factor
    /// @param o_quantization_factor Output quantization factor
    /// @param with_activation Enable Relu activation.
    /// @param activation_slp Relu activation slope.
    fully_connected(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& weights,
        const primitive_id& bias,
        const primitive_id& w_quantization_factor,
        const float i_quantization_factor,
        const float o_quantization_factor,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
        )
        : primitive_base(id, { input }, output_padding)
        , weights(weights)
        , bias(bias)
        , weights_quantization_factors(w_quantization_factor)
        , output_calibration_factors("")
        , input_quantization_factor(i_quantization_factor)
        , output_quantization_factor(o_quantization_factor)
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
    {
    }

    /// @brief Constructs fully connected layer.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights Primitive id containing weights data.
    /// @param bias Primitive id containing bias data. Provide empty string if using Relu without bias.
    /// @param w_quantization_factor Primitive id containing weights quanitization factors per output feature map.
    /// @param output_calibration_factors Primitive id containing output calibration factors per output feature map.
    /// @param i_quantization_factor Input quantization factor
    /// @param with_activation Enable Relu activation.
    /// @param activation_slp Relu activation slope.
    fully_connected(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& weights,
        const primitive_id& bias,
        const primitive_id& w_quantization_factor,
        const primitive_id& output_calibration_factors,
        const float i_quantization_factor,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input }, output_padding)
        , weights(weights)
        , bias(bias)
        , weights_quantization_factors(w_quantization_factor)
        , output_calibration_factors(output_calibration_factors)
        , input_quantization_factor(i_quantization_factor)
        , output_quantization_factor(1.0f)
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{fully_connected}
    fully_connected(const dto* dto)
        :primitive_base(dto)
        , weights(dto->weights)
        , bias(dto->bias)
        , weights_quantization_factors(dto->weights_quantization_factors)
        , output_calibration_factors(dto->output_calibration_factors)
        , input_quantization_factor(dto->input_quantization_factor)
        , output_quantization_factor(dto->output_quantization_factor)
        , with_activation(dto->with_activation != 0)
        , activation_negative_slope(dto->activation_negative_slope)
    {
    }

    /// @brief Primitive id containing weights data.
    primitive_id weights;
    /// @brief Primitive id containing bias data.
    primitive_id bias;
    /// @brief Primitive id containing weights quanitization factors per output feature map.
    primitive_id weights_quantization_factors;
    /// @brief Primitive id containing output quanitization factors per output feature map.
    primitive_id output_calibration_factors;
    /// @brief Input quantization factor
    float input_quantization_factor;
    /// @brief Output quantization factor
    float output_quantization_factor;
    /// @brief Enable Relu activation.
    bool with_activation;
    /// @brief Relu activation slope.
    float activation_negative_slope;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override 
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        ret.push_back(weights);

        if (!bias.empty())
            ret.push_back(bias);

        if (!weights_quantization_factors.empty())
            ret.push_back(weights_quantization_factors);

        if (!output_calibration_factors.empty())
            ret.push_back(output_calibration_factors);

        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.weights = weights.c_str();
        dto.bias = bias.c_str();
        dto.weights_quantization_factors = weights_quantization_factors.c_str();
        dto.output_calibration_factors = output_calibration_factors.c_str();
        dto.input_quantization_factor = input_quantization_factor;
        dto.output_quantization_factor = output_quantization_factor;
        dto.with_activation = with_activation;
        dto.activation_negative_slope = activation_negative_slope;
    }
};
/// @}
/// @}
/// @}
}