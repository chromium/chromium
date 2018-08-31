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
#include "../C/average_unpooling.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs "average_unpooling" operation.
/// @details Reverse operation of average pooling.
/// Each element in every pooling window is filled with output / window size value. In case of window overlap the elements are added.
struct average_unpooling : public primitive_base<average_unpooling, CLDNN_PRIMITIVE_DESC(average_unpooling)>
{
    CLDNN_DECLARE_PRIMITIVE(average_unpooling)

    /// @brief Constructs average_unpooling primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_size Size of input for average pooling forward.
    /// @param stride Defines shift in output buffer.
    /// @param size Pooling kernel size.
    average_unpooling(
        const primitive_id& id,
        const primitive_id& input,
        const tensor output_size,
        const tensor& size,
        const tensor& stride,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input }, output_padding)
        , stride(stride)
        , size(size)
        , output_size(output_size)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{average_unpooling}
    average_unpooling(const dto* dto)
        : primitive_base(dto)
        , stride(dto->stride)
        , size(dto->size)
        , output_size(dto->output_size)
    {}

    /// @brief Defines shift in output buffer.
    tensor stride;
    /// @brief Pooling kernel size.
    tensor size;
    /// @brief Output size of this primitive.
    tensor output_size;

protected:

    void update_dto(dto& dto) const override
    {
        dto.stride = stride;
        dto.size = size;
        dto.output_size = output_size;
    }
};
/// @}
/// @}
/// @}
}