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
#ifndef CONCATENATION_H
#define CONCATENATION_H

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

typedef enum
{
    cldnn_concatenation_along_b = 0,
    cldnn_concatenation_along_f = CLDNN_TENSOR_BATCH_DIM_MAX,
    cldnn_concatenation_along_x = CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX,
    cldnn_concatenation_along_y = cldnn_concatenation_along_x + 1
} cldnn_concatenation_axis;

/// @details Concatenation is used to concatenate multiple sources into one destination along specified dimension.
/// Note that all other dimensions (except the one along which concatenation take place) must have the same value in each source
/// and each source should have the same format.
/// @par Alogrithm:
/// \code
///     int outputIdx = 0
///     for(i : input)
///     {
///         for(f : i.features)
///         {
///             output[outputIdx] = f
///             outputIdx += 1
///         }
///     }
/// \endcode
/// @par Where: 
///   @li input : data structure holding all source inputs for this primitive
///   @li output : data structure holding output data for this primitive
///   @li i.features : number of features in currently processed input
///   @li outputIdx : index of destination feature 
CLDNN_BEGIN_PRIMITIVE_DESC(concatenation)
/// @brief Dimension along which concatenation should take place.
cldnn_concatenation_axis axis;
CLDNN_END_PRIMITIVE_DESC(concatenation)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(concatenation);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* CONCATENATION_H */

