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
#ifndef LRN_H
#define LRN_H

#include "cldnn.h"
/// @addtogroup c_api C API
/// @{
/// @addtogroup c_topology Network Topology
/// @{
/// @addtogroup c_primitives Primitives
/// @{

#ifdef __cplusplus
extern "C" {
#endif

typedef enum /*:int32_t*/
{
    cldnn_lrn_norm_region_across_channel,
    cldnn_lrn_norm_region_within_channel
} cldnn_lrn_norm_region;

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
CLDNN_BEGIN_PRIMITIVE_DESC(lrn)
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
CLDNN_END_PRIMITIVE_DESC(lrn)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(lrn);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* LRN_H */

