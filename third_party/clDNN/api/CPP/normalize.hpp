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
#include "../C/normalize.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Normalizes the input using an L2 norm and multiplies the output with scale value.
/// The scale can be equal for all channels or one scale per channel.
/// @details The L2 norm is computed as:<br>
/// Across spatial mode (across_spatial=true)-<br>
/// norm(i,x,y) = sqrt( &Sigma;( in(f,w,h)^2 ) + epsilon ) where f in range (0,num_of_features), w in range (0,input_width), h in range (0,input_height).<br>
/// The summation is performed over all the pixels in the batch.<br>
/// Within spatial mode (across_spatial=false)-<br>
/// norm(i,x,y) = sqrt( &Sigma;( in(f,x,y)^2 ) + epsilon ) where f in range (0,num_of_features).<br>
/// The summation is performed over this (x,y) position on all the features.<br>
/// @par Algorithm:
///   out(i,x,y) = ( in(i,x,y) / norm(i,x,y) ) * scale(i) 
/// @par Where:
///   @li out(i,x,y) : value at x, y from i-th feature map after normalization.
///   @li in(i,x,y) : value at x, y from i-th feature map before normalization.
///   @li norm(i,x,y) : L2 norm as described above.
///   @li scale(i) : the scale value of the i-th feature map.
struct normalize :public primitive_base<normalize, CLDNN_PRIMITIVE_DESC(normalize)>
{
    CLDNN_DECLARE_PRIMITIVE(normalize)

    /// @brief Constructs normalize primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param scale_input Scale input primitive id with values needed for scaling after the normalization.
    /// Scale x dimension should be 1 (if all channels have the same scale) or equal to input feature size (one scale per channel).
    /// All other dimensions should be 1.
    /// @param across_spatial Determines if the normalization is done across or within spatial (see documentation above).
    /// @param epsilon Epsilon for not dividing by zero while normalizing.
    normalize(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& scale_input,
        const bool across_spatial = true,
        const float epsilon = 1e-10f,
        const padding& output_padding = padding()
        )
        : primitive_base(id, {input}, output_padding)
        , scale_input(scale_input)
        , across_spatial(across_spatial)
        , epsilon(epsilon)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{normalize}
    normalize(const dto* dto)
        : primitive_base(dto)
        , scale_input(dto->scale_input)
        , across_spatial(dto->across_spatial != 0)
        , epsilon(dto->epsilon)
    {}

    /// @brief Scale input primitive id with values needed for scaling after the normalization.
    /// Scale x dimension should be 1 (if all channels have the same scale) or equal to input feature size (one scale per channel).
    /// All other dimensions should be 1.
    primitive_id scale_input;
    /// @brief Determines if the normalization is done across or within spatial (see documentation above).
    bool across_spatial;
    /// @brief Epsilon for not dividing by zero while normalizing.
    float epsilon;

protected:

    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        return{ scale_input };
    }

    void update_dto(dto& dto) const override
    {
        dto.scale_input = scale_input.c_str();
        dto.across_spatial = across_spatial;
        dto.epsilon = epsilon;
    }
};
/// @}
/// @}
/// @}
}