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
#include "../C/softmax_loss_grad.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Backward pass for Softmax log loss.
/// @details The output values are the same as input_prob, except for the correct one based on the label which is subtracted by 1.
struct softmax_loss_grad : public primitive_base<softmax_loss_grad, CLDNN_PRIMITIVE_DESC(softmax_loss_grad)>
{
    CLDNN_DECLARE_PRIMITIVE(softmax_loss_grad)

    /// @brief Constructs softmax_loss_grad primitive.
    /// @param id This primitive id.
    /// @param input_prob Input primitive id.
    /// @param labels Labels primitive id.
    softmax_loss_grad(
        const primitive_id& id,
        const primitive_id& input_prob,
        const primitive_id& labels,
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input_prob, labels }, output_padding)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{softmax_loss_grad}
    softmax_loss_grad(const dto* dto)
        :primitive_base(dto)
    {}

private:
    void update_dto(dto&) const override
    {
    }
};
/// @}
/// @}
/// @}
}