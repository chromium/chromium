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
#include "../C/concatenation.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @details Concatenation is used to concatenate multiple sources into one destination along specified dimension.
/// @notes 
/// - all other dimensions (except the one along which concatenation take place) must have the same value in each source.
/// - order of arguments in primitive creation has impact on order of feature maps in output primitive. 
/// 
/// @par Alogrithm:
/// \code
///     int outputIdx = 0
///     for(i : input)
///     {
///         for(f : i.features)
///         {
///             output[outputIdx] = f
///             outputIdx += 1
///         }
///     }
/// \endcode
/// @par Where: 
///   @li input : data structure holding all source inputs for this primitive
///   @li output : data structure holding output data for this primitive
///   @li i.features : number of features in currently processed input
///   @li outputIdx : index of destination feature 
struct concatenation : public primitive_base<concatenation, CLDNN_PRIMITIVE_DESC(concatenation)>
{
    CLDNN_DECLARE_PRIMITIVE(concatenation)

    enum concatenation_axis
    {
        along_b = cldnn_concatenation_along_b,
        along_f = cldnn_concatenation_along_f,
        along_x = cldnn_concatenation_along_x,
        along_y = cldnn_concatenation_along_y
    };

    /// @li Constructs concatenation primitive.
    /// @param id This primitive id.
    /// @param input Vector of input primitives ids.
    /// @param axis Selected dimension for concatenation.
    concatenation(
        const primitive_id& id,
        const std::vector<primitive_id>& input,
        const concatenation_axis axis,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input }, output_padding)
        , axis(axis)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC(depth_concatenate)
    concatenation(const dto* dto)
        :primitive_base(dto)
        , axis(static_cast<concatenation_axis>(dto->axis))
    {}

    /// @brief Dimension along which concatenation should take place
    concatenation_axis axis;

private:
    void update_dto(dto& dto) const override
    {
        dto.axis = static_cast<cldnn_concatenation_axis>(axis);
    }
};
/// @}
/// @}
/// @}
}
