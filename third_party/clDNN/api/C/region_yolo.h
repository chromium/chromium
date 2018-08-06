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
#ifndef REGION_YOLO_H
#define REGION_YOLO_H

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
    
    /// @brief region softmax specific for yolo2 topology
    /// @details
    /// @par Algorithm:
    ///   
    /// @par Where:
    ///   
    CLDNN_BEGIN_PRIMITIVE_DESC(region_yolo)
    /// @brief paramter coords
        uint32_t coords;
    /// @brief paramter classes
        uint32_t classes;
    /// @brief Number of anchors
        uint32_t num;
    /// @brief Apply softmax after logistic
        uint32_t do_softmax;
    /// @brief Number of really used anchors
        uint32_t mask_size;
    CLDNN_END_PRIMITIVE_DESC(region_yolo)

        CLDNN_DECLARE_PRIMITIVE_TYPE_ID(region_yolo);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* REGION_YOLO_H */

