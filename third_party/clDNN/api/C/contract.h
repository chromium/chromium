// Copyright (c) 2019 Intel Corporation
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

///////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CONTRACT_H
#define CONTRACT_H

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

    /// @brief Select reduction operation for contract layer ( @CLDNN_PRIMITIVE_DESC{contract} ?).
    typedef enum /*:int32_t*/
    {
        /// @brief Sum reduction.
        cldnn_contract_sum,
        /// @brief Product reduction.
        cldnn_contract_product,
        /// @brief All reduction.
        cldnn_contract_all,
        /// @brief Any reduction.
        cldnn_contract_any,
        /// @brief Max reduction.
        cldnn_contract_max
    } cldnn_contract_mode;

    /// @brief Reduces input with an operation defined by @p mode along defined
    ///        by @p reduction_axes dimensions.
    ///
    /// @details Reduces the input using the binary operation determined by
    ///          @p mode. The @p reduction_axes determine the final shape of the
    ///          output, which is calculated based on the input shape by
    ///          collapsing the dimensions along which the reduction happens.
    ///          For example, for the input with
    /// @n      <tt>input_sizes = (in_b, in_f, in_y, in_x)</tt>
    /// @n a reduction with
    /// @n      <tt>reduction_axes = (2)</tt>
    /// @n would collapse the Y dimension, producing
    /// @n      <tt>output_shape = (1, in_b, in_f, in_x)</tt>
    /// @n where every element is a @p mode reduction of the input elements with
    /// @n the same B, F and X coordinates.
    /// @n
    /// @n@b Requirements:
    /// @n - @p reduction_axes size (dimensions count) must be within (inclusive) range
    ///      1 - 4.
    /// @n - @p reduction_axes mustn't have duplicate values.
    /// @n - Values of @p reduction_axes must be within (inclusive) range 0 - 3
    /// @n Breaking any of these conditions will raise an exception.
    CLDNN_BEGIN_PRIMITIVE_DESC(contract)
    /// @brief Reduction mode. See #cldnn_contract_mode.
    int32_t mode; /*cldnn_contract_mode*/
    /// @brief Array of axes positions from input shape (0-based, from left to right)
    ///        along which reduction should happen.
    cldnn_uint16_t_arr reduction_axes;

    CLDNN_END_PRIMITIVE_DESC(contract)


        CLDNN_DECLARE_PRIMITIVE_TYPE_ID(contract);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif // CONTRACT_H
