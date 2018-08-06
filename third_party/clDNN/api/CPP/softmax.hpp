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
#include "../C/softmax.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Normalizes results so they sum to 1.
/// @details
/// @par Algorithm:
///   b = e^a/sum(N-1; j=0; e^j)
/// @par Where:
///   @li N : number of values to normalize
///   @li b : value after normalization
///   @li a : value before normalization
struct softmax : public primitive_base<softmax, CLDNN_PRIMITIVE_DESC(softmax)>
{
    CLDNN_DECLARE_PRIMITIVE(softmax)

    /// @brief Enum type to specify softmax's normalization scope (see #dimension).
    enum dimension_t
    {
        normalize_f = cldnn_softmax_normalize_f,
        normalize_x = cldnn_softmax_normalize_x,
        normalize_y = cldnn_softmax_normalize_y,
        normalize_fyx = cldnn_softmax_normalize_fyx,
    };

    /// @brief Constructs softmax primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param dimension Defines a scope of normalization (see #dimension).
    softmax(
        const primitive_id& id,
        const primitive_id& input,
        const dimension_t dimension = normalize_fyx,
        const padding& output_padding = padding()
    )
        :primitive_base(id, {input}, output_padding)
        , dimension(dimension)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{softmax}
    softmax(const dto* dto)
        :primitive_base(dto)
        , dimension(static_cast<dimension_t>(dto->dimension))
    {}

    /// @brief Defines a scope of a single softmax normalization.
    /// @details
    /// Being given a 4-dimensional input, which consists of b,f,y,x dimensions, softmax normalizes data which are divided into multiple independent sets.
    /// Specific behaviour is determined by this parameter, as follows:
    /// - when set to @link softmax::dimension_t softmax::normalize_x @endlink each input row is normalized independently,
    /// - when set to @link softmax::dimension_t softmax::normalize_y @endlink each input column is normalized independently,
    /// - when set to @link softmax::dimension_t softmax::normalize_f @endlink each in-depth vector of input is normalized independently,
    /// - when set to @link softmax::dimension_t softmax::normalize_fyx @endlink each 3d image within input is normalized independently,
    dimension_t dimension;

private:
    void update_dto(dto& dto) const override
    {
        dto.dimension = static_cast<cldnn_softmax_dimension>(dimension);
    }
};
/// @}
/// @}
/// @}
}