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

typedef enum /*:int32_t*/
{
    cldnn_lstm_offset_order_iofz = 0, // ONNX
    cldnn_lstm_offset_order_ifoz  // Caffe
} cldnn_lstm_offset_order;


/// @brief Performs forward Long Short-Term Memory (LSTM) layer.
/// @details The current implementation of LSTM supports Peepholes.
///   it = f(Xt*(Wi^T) + Ht-1*Ri + Pi (.) Ct-1 + Wbi + Rbi)
///   ft = f(Xt*(Wf^T) + Ht-1*Rf + Pf (.) Ct-1 + Wbf + Rbf)
///   ct = g(Xt*(Wc^T) + Ht-1*Rc + Wbc + Rbc)
///   Ct = ft (.) Ct-1 + it (.) ct
///   ot = f(Xt*(Wo^T) + Ht-1*Ro + Po (.) Ct + Wbo + Rbo)
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
/// @brief Weights, recurrent weights, and biases order. [iofz] : ONNX, [ifoz] : Caffe
cldnn_lstm_offset_order offset_order;
// NOT SUPPORTED YET
// /// @brief Number of directions default = 1, bidirectional = 2.
// uint32_t num_directions;
// /// @brief The sequence output for the hidden. This is not clearly specified in the ONNX definition.
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
// NOT SUPPORTED YET
// /// @brief Number of directions default = 1, bidirectional = 2.
// uint32_t num_directions;
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
// NOT SUPPORTED YET
// /// @brief Number of directions default = 1, bidirectional = 2.
// uint32_t num_directions;
// /// @brief The sequence output for the hidden. This is not clearly specified in the ONNX definition.
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

