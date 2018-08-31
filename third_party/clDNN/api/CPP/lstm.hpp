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
#include "../C/lstm.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs forward Long Short-Term Memory (LSTM) layer.
/// @details The current implementation of LSTM supports Peepholes.
///   it = f(Xt*(Wi^T) + Ht-1*Ri + Pi (.) Ct-1 + Wbi + Rbi)
///   ft = f(Xt*(Wf^T) + Ht-1*Rf + Pf (.) Ct-1 + Wbf + Rbf)
///   ct = g(Xt*(Wc^T) + Ht-1*Rc + Wbc + Rbc)
///   Ct = ft (.) Ct-1 + it (.) ct
///   ot = f(Xt*(Wo^T) + Ht-1*Ro + Po (.) Ct + Wbo + Rbo)
///   Ht = ot (.) h(Ct)
/// Where f=Sigmoid, g=Tanh, and h = Tanh.
struct lstm : public primitive_base<lstm, CLDNN_PRIMITIVE_DESC(lstm)>
{
    CLDNN_DECLARE_PRIMITIVE(lstm)

    /// @brief Constructs lstm layer.
    /// @param id This primitive id.
    /// @param input Vector of primitive id.
    /// @param weights Primitive id containing weights data.
    /// @param bias Primitive id containing bias data. Provide empty string if using lstm without bias.
    /// @param initial_hidden Primitive id containing initial_hidden data. Provide empty string if using lstm without initial_hidden values.
    /// @param initial_cell Primitive id containing initial_cell data. Provide empty string if using lstm without initial_cell values.
    /// @param peepholes Primitive id containing peepholes data. Provide empty string if using lstm without peepholes.
    /// @param clip Clip threshold. Provide 0 if using lstm without activations clip threshold.
    /// @param input_forget Provide 0 if using lstm without coupled input-forget gates.
    /// @param activations Vector of activations. Specify [f, g, h]. Default are [sigmoid, tanh, tanh]
    /// @param activation_params Vector of ativation params. Specify params for each [f, g, h] activation.
    /// @param offset_order Order of the concatenated weights, recurrent, and bias. ONNX default is iofz [input, output, forget, block].
    lstm(
        const primitive_id& id,
        const std::vector<primitive_id>& input,
        const primitive_id& weights,
        const primitive_id& recurrent,
        const primitive_id& bias = "",
        const primitive_id& initial_hidden = "",
        const primitive_id& initial_cell = "",
        const primitive_id& peepholes = "",
        const float clip = 0,
        const bool input_forget = 0,
        const std::vector<cldnn_activation_func>& activations = {},
        const std::vector<cldnn_activation_additional_params> activation_params = {},
        const cldnn_lstm_offset_order offset_order = cldnn_lstm_offset_order_iofz,
        const padding& output_padding = padding()
        )
        : primitive_base(id, input, output_padding)
        , weights(weights)
        , recurrent(recurrent)
        , bias(bias)
        , initial_hidden(initial_hidden)
        , initial_cell(initial_cell)
        , peepholes(peepholes)
        , clip(clip)
        , input_forget(input_forget)
        , activations(activations)
        , activation_params(activation_params)
        , offset_order(offset_order)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{lstm}
    lstm(const dto* dto)
        : primitive_base(dto)
        , weights(dto->weights)
        , recurrent(dto->recurrent)
        , bias(dto->bias)
        , initial_hidden(dto->initial_hidden)
        , initial_cell(dto->initial_cell)
        , peepholes(dto->peepholes)
        , clip(dto->clip)
        , input_forget(dto->input_forget)
        , activations(dto->activations, dto->activations + 3 * sizeof(cldnn_activation_func))
        , activation_params(dto->activation_params, dto->activation_params + 3 * sizeof(cldnn_activation_additional_params))
        , offset_order(dto->offset_order)
    {
    }

    /// @brief Primitive id containing weights data.
    primitive_id weights;
    /// @brief Primitive id containing recurrent data.
    primitive_id recurrent;
    /// @brief Primitive id containing bias data.
    primitive_id bias;
    /// @brief Primitive id containing the initial value of the hidden data.
    primitive_id initial_hidden;
    /// @brief Primitive id containing the initial value of the cell state data.
    primitive_id initial_cell;
    /// @brief Primitive id containing peepholes data.
    primitive_id peepholes;
    /// @brief Cell clip threshold T. It is applied to the input of activations [-T, T]. No clip is applied if it is not specified.
    float clip;
    /// @brief Couple the input and forget gates if input_forget is 1. Default is 0.
    bool input_forget;
    /// @brief A list of 3 activation functions for the input, output, forget, cell, and hidden.
    std::vector<cldnn_activation_func> activations;
    /// @brief Optional scaling values used by some activation functions. The values are consumed in the order of activation functions.
    std::vector<cldnn_activation_additional_params> activation_params;
    /// @brief Weights, recurrent weights, and biases order. [iofz] : ONNX, [ifoz] : Caffe
    cldnn_lstm_offset_order offset_order;

    // NOT SUPPORTED YET
    // /// @brief Number of directions default = 1, bidirectional = 2.
    // uint32_t num_directions;
    // /// @brief Optional tensor specifying lengths of the sequences in a batch.
    // /// If not specified - assumed all sequences in the batch to have length `seq_length`. It has shape `[batch_size]`.
    // tensor sequence_lens;
    // /// @brief The sequence output for the hidden??? This is not clearly specified in the ONNX definition.
    // uint32_t output_sequence;
protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        ret.push_back(weights);
        ret.push_back(recurrent);
        if (!bias.empty())
        {
            ret.push_back(bias);
        }
        if (!initial_hidden.empty())
        {
            ret.push_back(initial_hidden);
        }
        if (!initial_cell.empty())
        {
            ret.push_back(initial_cell);
        }
        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.weights = weights.c_str();
        dto.recurrent = recurrent.c_str();
        dto.bias = bias.c_str();
        dto.peepholes = peepholes.c_str();
        dto.initial_hidden = initial_hidden.c_str();
        dto.initial_cell = initial_cell.c_str();
        dto.offset_order = offset_order;
        if (activations.size() == 3) {
            std::copy_n(activations.begin(), 3, dto.activations);
        }
        if (activation_params.size() == 3) {
            std::copy_n(activation_params.begin(), 3, dto.activation_params);
        }
        dto.clip = clip;
        dto.input_forget = input_forget;
    }
};

struct lstm_gemm : public primitive_base<lstm_gemm, CLDNN_PRIMITIVE_DESC(lstm_gemm)>
{
    CLDNN_DECLARE_PRIMITIVE(lstm_gemm)

    /// @brief Constructs lstm layer.
    /// @param id This primitive id.
    /// @param input input primitive id.
    /// @param input weights Primitive id containing weights data.
    /// @param input recurrent Primitive id containing recurrent data. It is required even for no hidden values.
    /// @param input bias Primitive id containing bias data. Provide empty string if using lstm without bias.
    /// @param input hidden Primitive id containing hidden data. Provide empty string if using lstm without hidden values.
    lstm_gemm(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& weights,
        const primitive_id& recurrent,
        const primitive_id& bias = "",
        const primitive_id& hidden = "",
        const padding& output_padding = padding()
        )
        : primitive_base(id, {input}, output_padding)
        , weights(weights)
        , recurrent(recurrent)
        , bias(bias)
        , hidden(hidden)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{lstm}
    lstm_gemm(const dto* dto)
        : primitive_base(dto)
        , weights(dto->weights)
        , recurrent(dto->recurrent)
        , bias(dto->bias)
        , hidden(dto->hidden)
    {
    }

    /// @brief Primitive id containing weights data.
    primitive_id weights;
    /// @brief Primitive id containing recurrent data.
    primitive_id recurrent;
    /// @brief Primitive id containing bias data.
    primitive_id bias;
    /// @brief Primitive id containing the initial value of the hidden data.
    primitive_id hidden;
    /// @brief Number of directions default = 1, bidirectional = 2.
    uint32_t num_directions;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        ret.push_back(weights);
        ret.push_back(recurrent);
        if (!bias.empty())
            ret.push_back(bias);
        if (!hidden.empty())
            ret.push_back(hidden);
        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.weights = weights.c_str();
        dto.recurrent = recurrent.c_str();
        dto.bias = bias.c_str();
        dto.hidden = hidden.c_str();
    }
};

struct lstm_elt : public primitive_base<lstm_elt, CLDNN_PRIMITIVE_DESC(lstm_elt)>
{
    CLDNN_DECLARE_PRIMITIVE(lstm_elt)
    using vec_activation = std::vector<cldnn_activation_func>;
    using vec_activation_param = std::vector<cldnn_activation_additional_params>;

    /// @brief Constructs lstm layer.
    /// @param id This primitive id.
    /// @param input input primitive id.
    /// @param input cell Primitive id containing cell data. Provide empty string if using lstm without cell values.
    /// @param clip Clip threshold. Provide 0 if using lstm without activations clip threshold.
    /// @param input_forget Provide 0 if using lstm without coupled input-forget gates.
    /// @param offset_order. Order of the concatenated weights, recurrent, and bias. ONNX default is iofz [input, output, forget, block].
    lstm_elt(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& cell = "",
        const float clip = 0,
        const bool input_forget = 0,
        const std::vector<cldnn_activation_func> activations = {},
        const std::vector<cldnn_activation_additional_params> activation_params = {},
        const cldnn_lstm_offset_order offset_order = cldnn_lstm_offset_order_iofz,
        const padding& output_padding = padding()
        )
        : primitive_base(id, {input}, output_padding)
        , cell(cell)
        , clip(clip)
        , input_forget(input_forget)
        , activations(activations)
        , activation_params(activation_params)
        , offset_order(offset_order)
    {
    }

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{lstm}
    lstm_elt(const dto* dto)
        : primitive_base(dto)
        , cell(dto->cell)
        , clip(dto->clip)
        , input_forget(dto->input_forget)
        , activations(dto->activations, dto->activations + 3 * sizeof(cldnn_activation_func))
        , activation_params(dto->activation_params, dto->activation_params + 3 * sizeof(cldnn_activation_additional_params))
        , offset_order(dto->offset_order)
    {
    }

    /// @brief Primitive id containing the initial value of the cell state data.
    primitive_id cell;
    /// @brief Cell clip threshold T. It is applied to the input of activations [-T, T]. No clip is applied if it is not specified.
    float clip;
    /// @brief Couple the input and forget gates if input_forget is 1. Default is 0.
    bool input_forget;
    /// @brief A list of 3 activation functions for the input, output, forget, cell, and hidden.
    std::vector<cldnn_activation_func> activations;
    /// @brief Optional scaling values used by some activation functions. The values are consumed in the order of activation functions.
    std::vector<cldnn_activation_additional_params> activation_params;
    /// @brief Weights, recurrent weights, and biases order. [iofz] : ONNX, [ifoz] : Caffe
    cldnn_lstm_offset_order offset_order;

    // NOT SUPPORTED YET
    // /// @brief Number of directions default = 1, bidirectional = 2.
    // uint32_t num_directions;
protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret;
        if (!cell.empty())
            ret.push_back(cell);
        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.cell = cell.c_str();
        dto.offset_order = offset_order;
        dto.clip = clip;
        dto.input_forget = input_forget;
        if (activations.size() == 3) {
            std::copy_n(activations.begin(), 3, dto.activations);
        }
        if (activation_params.size() == 3) {
            std::copy_n(activation_params.begin(), 3, dto.activation_params);
        }
    }
};

/// @}
/// @}
/// @}
}