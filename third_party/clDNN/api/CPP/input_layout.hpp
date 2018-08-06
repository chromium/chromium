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
#include "../C/input_layout.h"
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

/// @brief Provides input layout for a data to be passed later to network.
/// @details This primitive allows to define the layout for input data
/// which will be passed to network before execution.
/// For example, network input images.
/// @note User should call network::set_input_data() for every @p input_layout primitive before network execution.
/// @note @p output_padding property of @p input_layout is ignored - its output layout is always equal to input layout defined during object creation.
/// @sa network::set_input_data(), cldnn::data
struct input_layout : public primitive_base<input_layout, CLDNN_PRIMITIVE_DESC(input_layout)>
{
    CLDNN_DECLARE_PRIMITIVE(input_layout)

    /// @brief Constructs input layout primitive.
    /// @param id This primitive id.
    /// @param layout Defines layout for the data will be passed to network.
    input_layout(const primitive_id& id, const layout& layout)
        :primitive_base(id, {}, layout.data_padding)
        , layout(layout)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{input_layout}
    explicit input_layout(const dto* dto)
        :primitive_base(dto)
        , layout(dto->layout)
    {
        output_padding = layout.data_padding;
    }

    /// @brief Defines layout for the data will be passed to network.
    mutable cldnn::layout layout;

    void change_layout(cldnn::layout new_layout)
    {
        layout = new_layout;
    }

private:
    void update_dto(dto& dto) const override
    {
        dto.layout = layout;
    }
};
/// @}
/// @}
/// @}
}
