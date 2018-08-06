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
#include "../C/mvn.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Mean Variance Normalization primitive.
/// @details Normalizes the input to have 0-mean and/or unit (1) variance.
struct mvn :public primitive_base<mvn, CLDNN_PRIMITIVE_DESC(mvn)>
{
    CLDNN_DECLARE_PRIMITIVE(mvn)

    /// @brief Constructs mvn primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param across_channels Determines if the normalization is done across or within channels. Default is within channels.'
    /// @param normalize_variance Determines if normalize variance is applied. Default is true.
    /// @param epsilon Epsilon for not dividing by zero while normalizing.
    mvn(
        const primitive_id& id,
        const primitive_id& input,
        const bool across_channels = false,
        const bool normalize_variance = true,
        const float epsilon = 1e-10f,
        const padding& output_padding = padding()
        )
        : primitive_base(id, {input}, output_padding)
        , across_channels(across_channels)
        , normalize_variance(normalize_variance)
        , epsilon(epsilon)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{mvn}
    mvn(const dto* dto)
        : primitive_base(dto)
        , across_channels(dto->across_channels != 0)
        , normalize_variance(dto->normalize_variance != 0)
        , epsilon(dto->epsilon)
    {}

    /// @brief Determines if the normalization is done across or within channels.
    bool across_channels;
    /// @brief Determines if normalize variance is applied.
    bool normalize_variance;
    /// @brief Epsilon for not dividing by zero while normalizing.
    float epsilon;

protected:

    void update_dto(dto& dto) const override
    {
        dto.across_channels = across_channels;
        dto.normalize_variance = normalize_variance;
        dto.epsilon = epsilon;
    }
};
/// @}
/// @}
/// @}
}