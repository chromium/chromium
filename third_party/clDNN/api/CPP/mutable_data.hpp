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
#include "../C/mutable_data.h"
#include "primitive.hpp"
#include "memory.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Provides mutable data.
/// @details This primitive allows to pass data which can be written to during training.
/// For example, weights and biases for scoring networks.
/// This primitive can be also set as other primitive's output. In this case the underlying buffer will be the same in mutable_data and preceding primitive.
struct mutable_data : public primitive_base<mutable_data, CLDNN_PRIMITIVE_DESC(mutable_data)>
{
    CLDNN_DECLARE_PRIMITIVE(mutable_data)

    /// @brief Enum type to specify function for data filling.
    enum filler_type
    {
        no_fill,
        zero,
        xavier
    };

    /// @brief Constructs mutable_data primitive.
    /// @param id This primitive id.
    /// @param mem @ref memory object which contains data.
    /// @param filler_type @ref data filling function, default is zero
    /// @note If memory is attached by memory::attach(), the attached buffer should be valid till network build.
    mutable_data(const primitive_id& id, const memory& mem, filler_type fill_type = filler_type::no_fill)
        :primitive_base(id, {}, padding())
        , mem(mem)
        , fill_type(fill_type)
    {}

    /// @brief Constructs mutable_data primitive with inputs.
    /// @param id This primitive id.
    /// @param input Vector of input primitives ids.
    /// @param mem @ref memory object which contains data.
    /// @note If memory is attached by memory::attach(), the attached buffer should be valid till network build.
    /// @param filler_type @ref data filling function, default is zero
    mutable_data(const primitive_id& id, const std::vector<primitive_id>& input, const memory& mem, filler_type fill_type = filler_type::no_fill)
        :primitive_base(id, { input }, padding())
        , mem(mem)
        , fill_type(fill_type)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{mutable_data}
    explicit mutable_data(const dto* dto)
        :primitive_base(dto)
        , mem(dto->mem)
        , fill_type(static_cast<filler_type>(dto->fill_type))
    {
        mem.retain();
    }

    /// @brief @ref memory object which contains data.
    /// @note If memory is attached by memory::attach(), the attached buffer should be valid till network build.
    memory mem;

    /// @brief Specifies function which will be used to fill weights.
    filler_type fill_type;

protected:
    void update_dto(dto& dto) const override
    {
        dto.mem = mem.get();
        dto.fill_type = static_cast<cldnn_filler_type>(fill_type);
    }
};
/// @}
/// @}
/// @}
}
