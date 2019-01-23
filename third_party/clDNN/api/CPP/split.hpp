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
#include "../C/split.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Performs split operation on input.
/// @details splits the input data into n parts, for each user provides name and offsets.
/// @n User cannot use split primitive directly.
/// @n It is needed to refer to the output ids with the name "<split_prim_id>:<split_output_id>".
/// @n
/// @n\b Assumptions 
/// @n - offsets1 < offsets2 < offsets3 < ...
/// @n - size[n] = offsets[n+1] - offsets[n];
/// @n - last element: size[n] = split_input.size - offsets[n];
/// @n - no buffer overlapping, as the output size is calculated using offset and input size
/// @n - split primitive id cannot be used by any other primitive (user needs to use output_ids only)
/// @n Breaking any of this conditions will cause exeption throw.
/// @n
/// @n\b Example:
/// @n Splitting output to 2 parts by the features:
/// @n input_size = { 2, 4, 3, 5 };
/// @n split_id = "split";
/// @n output_ids_offsets[0] = { "out0", { 0,0,0,0 } };
/// @n output_ids_offsets[1] = { "out1", { 0,2,0,0 } };
/// @n After split there would be 2 primitives: "split:out0" and "split:out1" which contain 2 feature maps (lower and upper)
struct split : public primitive_base<split, CLDNN_PRIMITIVE_DESC(split)>
{
    CLDNN_DECLARE_PRIMITIVE(split)

    /// @brief Constructs split primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param output_ids_offsets Pairs of output_ids and offsets
    split(
        const primitive_id& id,
        const primitive_id& input,
        const std::vector<std::pair<primitive_id, tensor> >& output_ids_offsets,
        const padding& output_padding = padding()
    )
        :primitive_base(id, {input}, output_padding)
        , output_ids(_output_ids.cpp_ids)
        , output_offsets(extract_tensor_vector(output_ids_offsets))
        , _output_ids(extract_primitive_vector(output_ids_offsets))
        , _output_offsets(tensor_vector_to_cldnn_vector(output_offsets))
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{split}
    split(const dto* dto)
        :primitive_base(dto)
        , output_ids(_output_ids.cpp_ids)
        , output_offsets(tensor_arr_to_vector(dto->output_offsets))
        , _output_ids(dto->output_ids)
        , _output_offsets(tensor_arr_to_cldnn_vector(dto->output_offsets))
    {
    }

    /// @brief List of output_ids.
    fixed_size_vector_ref output_ids;
    /// @brief Array of tensors with offsets.
    std::vector<tensor> output_offsets;

protected:
    primitive_id_arr _output_ids;
    std::vector<cldnn_tensor> _output_offsets;

    void update_dto(dto& dto) const override
    {
        dto.output_ids = _output_ids.ref();
        dto.output_offsets = tensor_vector_to_arr(_output_offsets);
    }

    static std::vector<primitive_id> extract_primitive_vector(const std::vector<std::pair<primitive_id, tensor> >& stor)
    {
        std::vector<primitive_id> res;
        for (auto &stor_pair : stor)
            res.push_back(stor_pair.first);

        return res;
    }

    static std::vector<tensor> extract_tensor_vector(const std::vector<std::pair<primitive_id, tensor> >& stor)
    {
        std::vector<tensor> res;
        for (auto &stor_pair : stor)
            res.push_back(stor_pair.second);

        return res;
    }
};
/// @}
/// @}
/// @}
}
