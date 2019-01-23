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
#include "../C/apply_adam.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Apply Adam primitive.
/// @details Updates output using Adam algorithm. The output of this primitive should be mutable_data type in case user wants to update
/// variable accross network. If output is not mutable_data then it will be initialized with 0.
/// "Adam: A Method for Stochastic Optimization" by Diederik P. Kingma, Jimmy Ba
/// @n See: https://arxiv.org/abs/1412.6980
/// 
/// <b>Algorithm:</b>
/// @n float lr[t] = lr * sqrt(1 - beta2^t) / (1 - beta1^t);
/// @n float m[t] = beta1 * m[t-1] + (1 - beta1) * grad[t];
/// @n float v[t] = beta2 * v[t-1] + (1 - beta2) * grad[t] * grad[t];
/// @n float result = result - lr[t] * m[t] / (sqrt(v[t]) + epsilon);

struct apply_adam : public primitive_base<apply_adam, CLDNN_PRIMITIVE_DESC(apply_adam)>
{
    CLDNN_DECLARE_PRIMITIVE(apply_adam)

    /// @brief Constructs apply Adam primitive.
    /// @param id This primitive id.
    /// @param input Input gradient primitive id.
    /// @param m Primitive id containing mean data.
    /// @param v Primitive id containing variance.
    /// @param beta1_power Primitive id containing beta1^t.
    /// @param beta2_power Primitive id containing beta2^t.
    /// @param lr Learning rate parameter.
    /// @param beta1 Beta1 parameter.
    /// @param beta2 Beta2 parameter.
    /// @param epsilon Epsilon.
    /// @param dependency_id Optional primitive id that need to complete before execution of this primitive. Used only for synchronization.
    apply_adam(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& m,
        const primitive_id& v,
        const primitive_id& beta1_power,
        const primitive_id& beta2_power,
        float lr,
        float beta1,
        float beta2,
        float epsilon,
        const primitive_id& dependency_id = "",
        const padding& output_padding = padding()
    )
        :primitive_base(id, {input}, output_padding)
        , m(m)
        , v(v)
        , beta1_power(beta1_power)
        , beta2_power(beta2_power)
        , lr(lr)
        , beta1(beta1)
        , beta2(beta2)
        , epsilon(epsilon)
        , dependency_id(dependency_id)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{apply_adam}
    apply_adam(const dto* dto)
        :primitive_base(dto)
        , m(dto->m)
        , v(dto->v)
        , beta1_power(dto->beta1_power)
        , beta2_power(dto->beta2_power)
        , lr(dto->lr)
        , beta1(dto->beta1)
        , beta2(dto->beta2)
        , epsilon(dto->epsilon)
        , dependency_id(dto->dependency_id)
    {
    }

    /// @brief Primitive id containing m data.
    primitive_id m;
    /// @brief Primitive id containing v data.
    primitive_id v;
    /// @brief Primitive id containing beta1^t.
    primitive_id beta1_power;
    /// @brief Primitive id containing beta2^t.
    primitive_id beta2_power;
    /// @brief Learning rate parameter.
    float lr;
    /// @brief Beta1 parameter.
    float beta1;
    /// @brief Beta2 parameter.
    float beta2;
    /// @brief Epsilon.
    float epsilon;
    /// @brief Optional primitive id that need to complete before execution of this primitive. Used only for synchronization.
    primitive_id dependency_id;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override
    {
        std::vector<std::reference_wrapper<const primitive_id>> ret{ m, v, beta1_power, beta2_power };
        ret.reserve(!dependency_id.empty());
        if (!dependency_id.empty())
            ret.push_back(dependency_id);
        return ret;
    }

    void update_dto(dto& dto) const override
    {
        dto.m = m.c_str();
        dto.v = v.c_str();
        dto.beta1_power = beta1_power.c_str();
        dto.beta2_power = beta2_power.c_str();
        dto.lr = lr;
        dto.beta1 = beta1;
        dto.beta2 = beta2;
        dto.epsilon = epsilon;
        dto.dependency_id = dependency_id.c_str();
    }
};
/// @}
/// @}
/// @}
}
