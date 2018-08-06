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
#include "../C/deconvolution.h"
#include "deconvolution.hpp"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs backward convolution operation for input.
/// @details convolution_grad_input is similar to deconvolution layer without biases and activation support. It actually uses deconvolution primitive underneath with gradient bool set to true.
struct convolution_grad_input : public deconvolution
{
    /// @brief Constructs convolution_grad_input primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_input window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    convolution_grad_input(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0,0,0,0 },
        const padding& output_padding = padding()
    )
        :deconvolution(id, input, { weights }, stride, input_offset, false, 0.0f, output_padding, true)
    {
    }

    /// @brief Constructs convolution_grad_input primitive (computes input paddings to match output size).
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_input window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    /// @param output_size User-defined output data size of the primitive (w/o padding).
    convolution_grad_input(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        tensor stride,
        tensor input_offset,
        tensor output_size,
        const padding& output_padding = padding()
    )
        :deconvolution(id, input, { weights }, stride, input_offset, false, 0.0f, output_size, output_padding, true)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{convolution_grad_input}
    convolution_grad_input(const dto* dto)
        :deconvolution(dto)
    {
    }

    /// @brief Constructs convolution_grad_input primitive (computes input paddings to match output size).
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param weights List of primitive ids containing weights data.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the convolution_grad_input window should start calculations.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    /// @param output_size User-defined output data size of the primitive (w/o padding).
    /// @return convolution_grad_input primitive with specified settings.
    static convolution_grad_input create_with_output_size(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<primitive_id>& weights,
        tensor output_size,
        tensor stride = { 1, 1, 1, 1 },
        tensor input_offset = { 0,0,0,0 },
        const padding& output_padding = padding()
    )
    {
        return convolution_grad_input(id, input, weights, stride, input_offset, output_size, output_padding);
    }

};
/// @}
/// @}
/// @}
}