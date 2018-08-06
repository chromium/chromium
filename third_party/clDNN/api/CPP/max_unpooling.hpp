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
#include "../C/max_unpooling.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs "max_unpooling" operation.
/// @details Reverse operation of max pooling, based on the argmax data where indices of each max pooling region are stored.
struct max_unpooling : public primitive_base<max_unpooling, CLDNN_PRIMITIVE_DESC(max_unpooling)>
{
    CLDNN_DECLARE_PRIMITIVE(max_unpooling)

    /// @brief Constructs max_unpooling primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param argmax Primitive id which contains indices of each max pooling region. Indices must be in flattened bfyx format with no padding. Needs to be fp32 data type.
    /// @param stride Defines shift in input buffer between adjacent calculations of output values. Used only for output size computation.
    /// @param size Pooling kernel size. Used only for output size computation.
    /// @param input_offset Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the pooling window should start calculations. Used only for output size computation.
    max_unpooling(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& argmax,
        const tensor& size,
        const tensor& stride,
        const tensor& input_offset = { 0,0,0,0 },
        const padding& output_padding = padding()
        )
        : primitive_base(id, {input}, output_padding)
        , argmax(argmax)
        , input_offset(input_offset)
        , stride(stride)
        , size(size)
        , with_output_size(false)
    {}

    /// @brief Constructs max_unpooling primitive (with provided output size)
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param argmax Primitive id which contains indices of each max pooling region. Indices must be in flattened bfyx format with no padding. Needs to be fp32 data type.
    /// @param output_size User-defined output data size of the primitive (w/o padding).
    max_unpooling(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& argmax,
        tensor output_size,
        const padding& output_padding = padding()
    )
        : primitive_base(id, { input }, output_padding)
        , argmax(argmax)
        , with_output_size(true)
        , output_size(output_size)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{max_unpooling}
    max_unpooling(const dto* dto)
        : primitive_base(dto)
        , argmax(dto->argmax)
        , input_offset(dto->input_offset)
        , stride(dto->stride)
        , size(dto->size)
        , with_output_size(dto->with_output_size != 0)
        , output_size(dto->output_size)
    {}

    /// @brief Primitive id which contains indices of each max pooling region. Indices must be in flattened bfyx format with no padding. Needs to be fp32 data type.
    primitive_id argmax;
    /// @brief Defines a shift, relative to (0,0) position of the input buffer, where (0,0) point of the pooling window should start calculations.
    tensor input_offset;
    /// @brief Defines shift in input buffer between adjacent calculations of output values. Used only for output size computation.
    tensor stride;
    /// @brief Pooling kernel size. Used only for output size computation.
    tensor size;
    /// @brief Indicates that the primitive has user-defined output size (non-zero value). Used only for output size computation.
    bool with_output_size;
    /// @brief User-defined output data size of the primitive (w/o padding).
    tensor output_size;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        return{ argmax };
    }

    void update_dto(dto& dto) const override
    {
        dto.argmax = argmax.c_str();
        dto.input_offset = input_offset;
        dto.stride = stride;
        dto.size = size;
        dto.with_output_size = with_output_size;
        dto.output_size = output_size;
    }
};
/// @}
/// @}
/// @}
}