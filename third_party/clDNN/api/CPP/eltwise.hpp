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
#include "../C/eltwise.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Select mode for the @ref eltwise layer.
enum class eltwise_mode : int32_t
{
    /// @brief Eltwise sum.
    sum = cldnn_eltwise_sum,
    /// @brief Eltwise subtract.
    sub = cldnn_eltwise_sub,
    /// @brief Eltwise max.
    max = cldnn_eltwise_max,
    /// @brief Eltwise product (Hadamard).
    prod = cldnn_eltwise_prod,
    /// @brief Eltwise div.
    div = cldnn_eltwise_div,
    /// @brief Eltwise min.
    min = cldnn_eltwise_min,
    /// @brief Eltwise pow.
    pow = cldnn_eltwise_pow,
    /// @brief Eltwise mod.
    mod = cldnn_eltwise_mod,
};

/// @brief Performs elementwise operations (sum, subtract, max or product) on two input primitives
/// Also supports built-in Relu @ref activation available by setting it in arguments.
/// @notes
/// - both inputs have to have equal sizes in all dimensions
/// - format of both inputs has to be the same
/// - when using integer types, only following eltwise modes are supported: sum, sub, prod, div
struct eltwise : public primitive_base<eltwise, CLDNN_PRIMITIVE_DESC(eltwise)>
{
    CLDNN_DECLARE_PRIMITIVE(eltwise)

    /// @brief Constructs eltwise primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param input2 Second input primitive id with values needed for eltwise computation.
    /// @param mode Eltwise mode.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    eltwise(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& input2,
        eltwise_mode mode,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input, input2 }, output_padding)
        , output_calibration_factors("")
        , output_quantization_factor(1.0f)
        , mode(mode)
        , coefficients(std::vector<float>(0))
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , stride(std::vector<tensor>(0))
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
    }

    /// @brief Constructs eltwise primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param input2 Second input primitive id with values needed for eltwise computation.
    /// @param stride Defines shift in input buffers between adjacent calculations of output values.
    /// @param mode Eltwise mode.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    eltwise(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& input2,
        std::vector<tensor> stride,
        eltwise_mode mode,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input, input2 }, output_padding)
        , output_calibration_factors("")
        , output_quantization_factor(1.0f)
        , mode(mode)
        , coefficients(std::vector<float>(0))
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , stride(stride)
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
    }

    /// @brief Constructs eltwise primitive.
    /// @param id This primitive id.
    /// @param inputs Input primitives ids.
    /// @param mode Eltwise mode.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    eltwise(
        const primitive_id& id,
        const std::vector<primitive_id>& inputs,
        eltwise_mode mode,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, inputs, output_padding)
        , output_calibration_factors("")
        , output_quantization_factor(1.0f)
        , mode(mode)
        , coefficients(std::vector<float>(0))
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , stride(std::vector<tensor>(0))
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
    }

    /// @brief Constructs eltwise primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param input2 Second input primitive id with values needed for eltwise computation.
    /// @param output_calibration_factors Primitive id containing output calibration factors per output feature map.
    /// @param mode Eltwise mode.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    eltwise(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& input2,
        const primitive_id& output_calibration_factors,
        eltwise_mode mode,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input, input2 }, output_padding)
        , output_calibration_factors(output_calibration_factors)
        , output_quantization_factor(1.0f)
        , mode(mode)
        , coefficients(std::vector<float>(0))
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , stride(std::vector<tensor>(0))
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
    }

    /// @brief Constructs eltwise primitive.
    /// @param id This primitive id.
    /// @param inputs Input primitives ids.
    /// @param output_calibration_factors Primitive id containing output calibration factors per output feature map.
    /// @param mode Eltwise mode.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    eltwise(
        const primitive_id& id,
        const std::vector<primitive_id>& inputs,
        const primitive_id& output_calibration_factors,
        eltwise_mode mode,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, inputs, output_padding)
        , output_calibration_factors(output_calibration_factors)
        , output_quantization_factor(1.0f)
        , mode(mode)
        , coefficients(std::vector<float>(0))
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , stride(std::vector<tensor>(0))
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
    }

    /// @brief Constructs eltwise primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param input2 Second input primitive id with values needed for eltwise computation.
    /// @param o_quantization_factor Output quantization factor
    /// @param mode Eltwise mode.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    eltwise(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& input2,
        const float o_quantization_factor,
        eltwise_mode mode,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input, input2 }, output_padding)
        , output_calibration_factors("")
        , output_quantization_factor(o_quantization_factor)
        , mode(mode)
        , coefficients(std::vector<float>(0))
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , stride(std::vector<tensor>(0))
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
    }

    /// @brief Constructs eltwise primitive.
    /// @param id This primitive id.
    /// @param inputs Input primitives ids.
    /// @param o_quantization_factor Output quantization factor
    /// @param mode Eltwise mode.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    eltwise(
        const primitive_id& id,
        const std::vector<primitive_id>& inputs,
        const float o_quantization_factor,
        eltwise_mode mode,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, inputs, output_padding)
        , output_calibration_factors("")
        , output_quantization_factor(o_quantization_factor)
        , mode(mode)
        , coefficients(std::vector<float>(0))
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , stride(std::vector<tensor>(0))
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
    }

    /// @brief Constructs eltwise primitive.
    /// @param id This primitive id.
    /// @param inputs Input primitives ids.
    /// @param coefficients Blob-wise coefficient for SUM operation
    /// @param mode Eltwise mode.
    /// @param with_activation Enables Relu activation.
    /// @param activation_slp Relu activation slope.
    eltwise(
        const primitive_id& id,
        const std::vector<primitive_id>& inputs,
        eltwise_mode mode,
        const std::vector<float>& coefficients,
        bool with_activation = false,
        float activation_slp = 0.0f,
        const padding& output_padding = padding()
    )
        :primitive_base(id, inputs, output_padding)
        , output_calibration_factors("")
        , output_quantization_factor(1.0f)
        , mode(mode)
        , coefficients(coefficients)
        , with_activation(with_activation)
        , activation_negative_slope(activation_slp)
        , stride(std::vector<tensor>(0))
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
        if (mode == eltwise_mode::sum && !coefficients.empty() && coefficients.size() != inputs.size())
        {
            throw std::invalid_argument("Invalid eltwise sum coefficients count (should be equal to 0 or input.size)");
        }
        if (mode != eltwise_mode::sum && !coefficients.empty())
        {
            throw std::invalid_argument("Only eltwise sum operation supports blob-wise coefficients");
        }
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{eltwise}
    eltwise(const dto* dto)
        :primitive_base(dto)
        , output_calibration_factors(dto->output_calibration_factors)
        , output_quantization_factor(dto->output_quantization_factor)
        , mode(static_cast<eltwise_mode>(dto->mode))
        , coefficients(float_arr_to_vector(dto->coefficients))
        , with_activation(dto->with_activation != 0)
        , activation_negative_slope(dto->activation_negative_slope)
        , stride(tensor_arr_to_vector(dto->stride))
        , _stride(tensor_vector_to_cldnn_vector(stride))
    {
        if (dto->input.size < 2)
            throw std::invalid_argument("eltiwise dto should containt at least two inputs");
        if (dto->coefficients.size != 0 && dto->coefficients.size != dto->input.size)
            throw std::invalid_argument("Invalid eltwise coefficients count in dto (should be equal to 0 or input.size)");
    }

    /// @brief Primitive id containing output quanitization factors per output feature map.
    primitive_id output_calibration_factors;
    /// @brief Output quantization factor
    float output_quantization_factor;
    /// @param mode Eltwise mode.
    eltwise_mode mode;
    /// @param coefficients Blob-wise coefficient for SUM operation.
    std::vector<float> coefficients;
    /// @brief Enables Relu activation.
    bool with_activation;
    /// @brief Relu activation slope.
    float activation_negative_slope;
    /// @brief Defines shift in input buffers between adjacent calculations of output values.
    std::vector<tensor> stride;

protected:
    std::vector<cldnn_tensor> _stride;
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        if(!output_calibration_factors.empty())
            ret.push_back(output_calibration_factors);

        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.output_calibration_factors = output_calibration_factors.c_str();
        dto.output_quantization_factor = output_quantization_factor;
        dto.mode = static_cast<cldnn_eltwise_mode>(mode);
        dto.coefficients = float_vector_to_arr(coefficients);
        dto.with_activation = with_activation;
        dto.activation_negative_slope = activation_negative_slope;
        dto.stride = tensor_vector_to_arr(_stride);
    }
};
/// @}
/// @}
/// @}
}
