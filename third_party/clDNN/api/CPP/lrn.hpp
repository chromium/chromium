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
#include "../C/lrn.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Local response normalization
/// @details LRN layer as described in chapter 3.3 of "ImageNet Classification with Deep Convolutional
/// Neural Networks" by Khrizevsky, Sutskever, Hinton. @n See: http://www.cs.toronto.edu/~fritz/absps/imagenet.pdf
/// @par Alogrithm:
///   b(i,x,y) = a(i,x,y) / (k+alpha*sum(min(N-1, i+n/2); j=max(0,i-n/2); a(j,x,y)^2))
/// @par Where:
///   @li b(i,x,y) : value at x, y from i-th feature map after normalization
///   @li a(i,x,y) : value at x, y from i-th feature map before normalization
///   @li N : number of feature maps
///   @li n : size of normalization
///   @li k, alpha, beta : hyper parameters (equal to 2, 10e-4, 0.75 in paper).
struct lrn :public primitive_base<lrn, CLDNN_PRIMITIVE_DESC(lrn)>
{
    CLDNN_DECLARE_PRIMITIVE(lrn)

    /// @brief Constructs LRN primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param size Size of normalization.
    /// @param k Hyper parameter "k".
    /// @param alpha Hyper parameter "alpha".
    /// @param beta Hyper parameter "beta".
    /// @param lrn_norm_region Normalize across or within channel
    lrn(
        const primitive_id& id,
        const primitive_id& input,
        uint32_t size,
        float k,
        float alpha,
        float beta,
        cldnn_lrn_norm_region lrn_norm_region,
        const padding& output_padding = padding()
        )
        : primitive_base(id, {input}, output_padding)
        , size(size)
        , k(k)
        , alpha(alpha)
        , beta(beta)
        , norm_region(lrn_norm_region)
    {}

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{normalization}
    lrn(const dto* dto)
        : primitive_base(dto)
        , size(dto->size)
        , k(dto->k)
        , alpha(dto->alpha)
        , beta(dto->beta)
        , norm_region(dto->norm_region)
    {}

    /// @brief Size of normalization.
    uint32_t size;
    /// @brief Hyper parameter "k".
    float k;
    /// @brief Hyper parameter "alpha".
    float alpha;
    /// @brief Hyper parameter "beta".
    float beta;
    /// @brief Normalize across or within channel
    cldnn_lrn_norm_region norm_region;

protected:
    void update_dto(dto& dto) const override
    {
        dto.size = size;
        dto.k = k;
        dto.alpha = alpha;
        dto.beta = beta;
        dto.norm_region = norm_region;
    }
};
/// @}
/// @}
/// @}
}