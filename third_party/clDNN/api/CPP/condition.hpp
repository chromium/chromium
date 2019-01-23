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

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../C/condition.h"
#include "primitive.hpp"
#include "topology.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{
/// @brief Function, which will be used during comparison.
enum cond_functions : int32_t
{
    EQUAL,
    GREATER,
    LESS
};

/// @brief Adds primitive, which works like "if".
///
/// @details
/// @n   Applies comparision between 2 inputs.
/// @n   Compare data - sizes of that input specifes the range of the comparison.
/// @n   Offset - offset in memory, when comparing values.
struct condition : public primitive_base<condition, CLDNN_PRIMITIVE_DESC(condition)>
{
    CLDNN_DECLARE_PRIMITIVE(condition)

        /// @brief Constructs condition primitive / layer.
        ///
        /// @param id                 An identifier of new primitive.
        /// @param input              An identifier of primitive which is an input for newly created
        ///                           condition primitive.
        /// @param topology_true      Topolgoy containg primitives, which will be executed when comparsion results  
        ///                           true.
        /// @param topology_false     Topolgoy containg primitives, which will be executed when comparsion results  
        ///                           false..
        /// @param compare_Data       An identifier of primitive which contains compare values
        /// @param func               Used function during comparison.
        /// @param offseg             Offset for compare data.
        /// @param output_padding     Optional padding for output from primitive.
        condition(
            const primitive_id& id,
            const primitive_id& input,
            const topology& topology_true,
            const topology& topology_false,
            const primitive_id& compare_data,
            const cond_functions& func,
            const tensor& offset = { 0, 0, 0, 0 },
            const padding& output_padding = padding()
        )
        : primitive_base(id, { input }, output_padding)
        , topology_true(topology_true)
        , topology_false(topology_false)
        , compare_data(compare_data)
        , function(func)
        , offset(offset)
    {}


    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{condition}
    condition(const dto* dto)
        : primitive_base(dto)
        , topology_true(dto->topology_true)
        , topology_false(dto->topology_false)
        , compare_data(dto->compare_data)
        , function(static_cast<cond_functions>(dto->function))
        , offset(dto->offset)
    {}


    /// @brief An identifier of topology, which will be executed when comparison returns true.
    topology topology_true;
    /// @brief An identifier of topology, which will be executed when comparison returns false.
    topology topology_false;
    /// @brief An identifier of primitive which contains compare values.
    primitive_id compare_data;
    /// @brief Used function during comparison.
    cond_functions function;
    /// @brief Offset for compare data.
    tensor offset;
protected:
    void update_dto(dto& dto) const override
    {
        dto.compare_data = compare_data.c_str();
        dto.function = static_cast<cldnn_cond_functions>(function);
        dto.offset = offset;
        dto.topology_true = topology_true.get();
        dto.topology_false = topology_false.get();
    }

    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        return { compare_data };
    }
};
}
/// @}
/// @}
/// @}