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
#include "../C/embed.h"
#include "primitive.hpp"

namespace cldnn
{
	/// @addtogroup cpp_api C++ API
	/// @{
	/// @addtogroup cpp_topology Network Topology
	/// @{
	/// @addtogroup cpp_primitives Primitives
	/// @{

	/// @brief 
	/// @details Performs embedding upon input. 
    /// @n\b Example:
    /// @n input_size = { 8, 1, 1, 75 };
    /// @n weights_size = {15, 1, 62, 1 };
    /// @n output_size = { 8, 75, 15, 1 };
	/// @par Algorithm:
	/// @par Where:
	struct embed : public primitive_base<embed, CLDNN_PRIMITIVE_DESC(embed)>
	{
		CLDNN_DECLARE_PRIMITIVE(embed)

			/// @brief Constructs embed primitive.
			/// @param id This primitive id.
			/// @param input Input primitive id.
            /// @param weights Primitive id containing weights data.
            /// @param bias Primitive id containing bias data.
		embed(
			const primitive_id& id,
			const primitive_id& input,
			const primitive_id& weights,
			const primitive_id& bias
			)
			: primitive_base(id, { input })
			, weights(weights)
			, bias(bias)
		{}

		/// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{embed}
		embed(const dto* dto)
			:primitive_base(dto)
			, weights(dto->weights)
			, bias(dto->bias)
		{
		}

		/// @brief Primitive id containing weights data.
		primitive_id weights;
		/// @brief Primitive id containing bias data.
		primitive_id bias;

	protected:
		std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
		{
			if (bias.empty())
				return{ weights };
			else
				return{ weights, bias };
		}

		void update_dto(dto& dto) const override
		{
			dto.weights = weights.c_str();
			dto.bias = bias.c_str();
		}

	};
	/// @}
	/// @}
	/// @}
}
#pragma once