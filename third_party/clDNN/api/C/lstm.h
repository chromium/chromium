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
#ifndef LSTM_H
#define LSTM_H

#include <stdbool.h>
#include "cldnn.h"
/// @addtogroup c_api C API
/// @{
/// @addtogroup c_topology Network Topology
/// @{
/// @addtogroup c_primitives Primitives
/// @{

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Weights orders
/// @details Specifies the order in which the weights are concatenated.
/// e.g. [i, o, f, z] : [input, output, forget, block]
/// ONNX order: iofz
/// Caffe order: ifoz
/// pyTorch order: izof
typedef enum /*:int32_t*/
{
    cldnn_lstm_offset_order_iofz = 0,
    cldnn_lstm_offset_order_ifoz,
    cldnn_lstm_offset_order_izof  
} cldnn_lstm_offset_order;

/// @brief LSTM Output selection
/// @details The current implementation allows the use to select the output
/// of an LSTM node by specifing any of the following options 
typedef enum /*:int32_t*/
{
	/// output the entire hidden sequence
	cldnn_lstm_output_sequence = 0,
	/// output just the last hidden value
	cldnn_lstm_output_hidden,
	/// output the last hidden and last cell values
	cldnn_lstm_output_hidden_cell,
	/// output the hidden sequence concatenated with the last cell
	cldnn_lstm_output_sequence_cell
} cldnn_lstm_output;

/// @brief Performs forward Long Short-Term Memory (LSTM) layer.
/// @details The current implementation of LSTM is described the following equations.
///   it = f(Xt*(Wi^T) + Ht-1*Ri + Wbi)
///   ft = f(Xt*(Wf^T) + Ht-1*Rf + Wbf)
///   ct = g(Xt*(Wc^T) + Ht-1*Rc + Wbc)
///   Ct = ft (.) Ct-1 + it (.) ct
///   ot = f(Xt*(Wo^T) + Ht-1*Ro + Wbo)
///   Ht = ot (.) h(Ct)
/// Where f = Sigmoid, g = Tanh, and h = Tanh.
CLDNN_BEGIN_PRIMITIVE_DESC(lstm)
/// @brief Array of primitive ids containing weight matrices for input, output, forget, and cell gates.
cldnn_primitive_id weights;
/// @brief Array of primitive ids containing recurrent weight matrices for input, output, forget, and cell gates.
cldnn_primitive_id recurrent;
/// @brief Array of primitive ids containing bias vectors for input, output, forget, and cell gates.
cldnn_primitive_id bias;
/// @brief Array of primitive ids containing the initial value of the hidden data (Ht-1).
cldnn_primitive_id initial_hidden;
/// @brief Array of primitive ids containing the initial value of the cell state data (Ct-1).
cldnn_primitive_id initial_cell;
/// @brief Array of primitive ids containing peephole weight vectors for input, output, and forget gates.
cldnn_primitive_id peepholes;
/// @brief Cell clip threshold T. It is applied to the input of activations [-T, T]. No clip is applied if it is not specified.
float clip;
/// @brief Couple the input and forget gates if input_forget is 1. Default is 0.
bool input_forget;
/// @brief A list of 3 activation functions for the input, output, forget, cell, and hidden.
cldnn_activation_func activations[3];
/// @brief Optional scaling values used by some activation functions. The values are consumed in the order of activation functions.
cldnn_activation_additional_params activation_params[3];
/// @brief Output selection. Default the entire hidden sequence is returned
cldnn_lstm_output output_selection;
/// @brief Weights, recurrent weights, and biases order. [iofz] : ONNX, [ifoz] : Caffe
cldnn_lstm_offset_order offset_order;
// NOT SUPPORTED YET
// uint32_t output_sequence;
CLDNN_END_PRIMITIVE_DESC(lstm)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(lstm);



/// @brief LSTM Layer GEMM helper primitive.
/// @details The current helper primitive performs fused GEMM operations.
CLDNN_BEGIN_PRIMITIVE_DESC(lstm_gemm)
/// @brief Array of primitive ids containing weight matrices for input, output, forget, and cell gates.
cldnn_primitive_id weights;
/// @brief Array of primitive ids containing recurrent weight matrices for input, output, forget, and cell gates.
cldnn_primitive_id recurrent;
/// @brief Array of primitive ids containing bias vectors for input, output, forget, and cell gates.
cldnn_primitive_id bias;
/// @brief Array of primitive ids containing the initial value of the hidden data (Ht-1).
cldnn_primitive_id hidden;
/// @brief direction default = 0, bidirectional = 1.
uint32_t direction;
CLDNN_END_PRIMITIVE_DESC(lstm_gemm)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(lstm_gemm);



/// @brief LSTM Layer element-wise helper primitive.
/// @details The current helper primitive performs fused element-wise operations.
CLDNN_BEGIN_PRIMITIVE_DESC(lstm_elt)
/// @brief Array of primitive ids containing the initial value of the cell state data (Ct-1).
cldnn_primitive_id cell;
/// @brief Cell clip threshold T. It is applied to the input of activations [-T, T]. No clip is applied if it is not specified.
float clip;
/// @brief Couple the input and forget gates if input_forget is 1. Default is 0.
bool input_forget;
/// @brief A list of 3 activation functions for the input, output, forget, cell, and hidden.
cldnn_activation_func activations[3];
/// @brief Optional scaling values used by some activation functions. The values are consumed in the order of activation functions.
cldnn_activation_additional_params activation_params[3];
/// @brief Weights, recurrent weights, and biases order. [iofz] : ONNX, [ifoz] : Caffe
cldnn_lstm_offset_order offset_order;
/// @brief direction default = 0, bidirectional = 1.
uint32_t direction;
// NOT SUPPORTED YET
// uint32_t output_sequence;
CLDNN_END_PRIMITIVE_DESC(lstm_elt)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(lstm_elt);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* LSTM_H */

