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
#pragma once

#include "../C/contract.h"
#include "primitive.hpp"


namespace cldnn
{
    /// @addtogroup cpp_api C++ API
    /// @{
    /// @addtogroup cpp_topology Network Topology
    /// @{
    /// @addtogroup cpp_primitives Primitives
    /// @{

    /// @brief Select mode for the @ref contract layer.
    enum class contract_mode : int32_t
    {
        /// @brief Sum reduction.
        sum = cldnn_contract_sum,
        /// @brief Product reduction.
        prod = cldnn_contract_product,
        /// @brief All reduction.
        all = cldnn_contract_all,
        /// @brief Any reduction.
        any = cldnn_contract_any,
        /// @brief Max reduction.
        max = cldnn_contract_max
    };

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
    struct contract : public primitive_base<contract, CLDNN_PRIMITIVE_DESC(contract)>
    {
        CLDNN_DECLARE_PRIMITIVE(contract)

            /// @brief Constructs contract primitive / layer.
            ///
            /// @param id              An identifier of new primitive.
            /// @param input           An identifier of primitive which is an input for newly created
            ///                        contract primitive.
            /// @param mode            Reduction mode.
            /// @param reduction_axes  Axes positions (0-based, from left to right) in input_shape
            ///                        that are being reduced.
            /// @param output_padding  Optional padding for output from primitive.
            contract(
                const primitive_id& id,
                const primitive_id& input,
                contract_mode mode,
                const std::vector<uint16_t>& reduction_axes = {},
                const padding& output_padding = padding()
            )
            : primitive_base(id, { input }, output_padding),
            mode(mode),
            reduction_axes(reduction_axes)
        {
        }

        /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{contract}
        contract(const dto* dto)
            : primitive_base(dto),
            mode(static_cast<contract_mode>(dto->mode)),
            reduction_axes(uint16_t_arr_to_vector(dto->reduction_axes))

        {
        }

        /// @param mode Contract mode.
        contract_mode mode;
        /// @brief Array of axes positions from input shape (0-based, from left to right)
        ///        along which reduction should happen.
        std::vector<uint16_t> reduction_axes;

    protected:
        void update_dto(dto& dto) const override
        {
            dto.mode = static_cast<cldnn_contract_mode>(mode);
            dto.reduction_axes = uint16_t_vector_to_arr(reduction_axes);
        }
    };
    /// @}
    /// @}
    /// @}
}