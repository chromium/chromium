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
#include "../C/select.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs elementwise select operation on two input primitives with selector primitive (mask)
/// @notes
/// - both inputs have to have equal sizes in all dimensions
/// - format of both inputs has to be the same
/// - mask primitive input have to have equal size in all dimensions with inputs
struct select : public primitive_base<select, CLDNN_PRIMITIVE_DESC(select)>
{
    CLDNN_DECLARE_PRIMITIVE(select)

    /// @brief Constructs select primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
	/// @param input2 Second input primitive id.
    /// @param mask Input primitive id with values needed for select computation.
    select(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& input2,
		const primitive_id& mask,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input, input2, mask }, output_padding)
	{
	}

	/// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{select}
	select(const dto* dto)
		:primitive_base(dto)
	{
	}

protected:
	void update_dto(dto&) const override
	{}
};
/// @}
/// @}
/// @}
}
