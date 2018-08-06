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
#include "../C/scale.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs elementwise product of input and scale_input.
/// @details Scale input dimension should be equal to input dimension or be 1 if it is not there.<br>
/// Input size : 2x3x4x5(BFYX)<br>
///     Possible scale inputs sizes :<br>
///     2x3x4x5 - works the same as(axis == 0 == -4) in caffe<br>
///     1x3x4x5 - works the same as(axis == 1 == -3) in caffe<br>
///     1x1x4x5 - works the same as(axis == 2 == -2) in caffe<br>
///     1x1x1x5 - works the same as(axis == 3 == -1) in caffe<br>
///     1x1x1x1 - works the same as empty shape(scalar) in caffe<br>
/// When scale_input is the same as input, the behavior is the same as @ref eltwise with product operation.<br>
/// Performs scale over feature when the scale feature size is equal to input feature size.<br>
/// Performs scale over feature in batch when the scale feature and scale batch sizes are equal to input feature and input batch sizes.<br>
/// Optionally it can also add provided biases by providing bias data.<br>
struct scale : public primitive_base<scale, CLDNN_PRIMITIVE_DESC(scale)>
{
    CLDNN_DECLARE_PRIMITIVE(scale)

    /// @brief Constructs scale primitive without adding bias.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param scale_input Scale input primitive id with values needed for product computation.
    scale(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& scale_input, //should be bfyx or yxfb, where each dimension can be 1, if all dimensions are 1 then this is scalar
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input, scale_input }, output_padding)
        , bias("")
    {
    }

    /// @brief Constructs scale primitive with optional adding bias.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param scale_input Scale input primitive id with values needed for product computation.
    /// @param bias Primitive id containing bias data.
    scale(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& scale_input, //should be bfyx or yxfb, where each dimension can be 1, if all dimensions are 1 then this is scalar
        const primitive_id& bias, //should be same size as scale_input
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input, scale_input }, output_padding)
        , bias(bias)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{scale}
    scale(const dto* dto)
        :primitive_base(dto)
        , bias(dto->bias)
    {
        if (dto->input.size != 2)
            throw std::invalid_argument("scale dto should contains exactly 2 inputs");
    }

    /// @brief Primitive id containing bias data.
    primitive_id bias;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    { 
        if (bias.empty())
            return{};
        else
            return{ bias };
    }

    void update_dto(dto& dto) const override
    {
        dto.bias = bias.c_str();
    }
};
/// @}
/// @}
/// @}
}
