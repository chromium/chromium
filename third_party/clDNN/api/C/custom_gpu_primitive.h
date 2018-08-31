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
#ifndef CUSTOM_GPU_PRIMITIVE_H
#define CUSTOM_GPU_PRIMITIVE_H

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

/// @brief This primitive executes a custom kernel provided by the application
/// @details The application is required to provide all relevant details for executing the custom kernel
/// such as: sources, entry point, work sizes and parameter bindings.
CLDNN_BEGIN_PRIMITIVE_DESC(custom_gpu_primitive)
/// @brief Source code for the kernel
cldnn_primitive_id_arr kernels_code;
/// @brief The name of the entry point function in the kernel
cldnn_kernel_entry_point kernel_entry_point;
/// @brief Argument bindings for the entry point function
cldnn_kernel_arguments kernel_arguments;
/// @brief The number of arguments used by the kernel
int kernel_arguments_num;
/// @brief The kernel's build options
cldnn_kernel_build_options build_options;
/// @brief The output layout declared by the primitive
cldnn_layout output_layout;
/// @brief The global working sizes
cldnn_work_group_sizes gws;
/// @brief The number of global work sizes
int gws_num;
/// @brief The local working sizes
cldnn_work_group_sizes lws;
/// @brief The number of local work sizes
int lws_num;

CLDNN_END_PRIMITIVE_DESC(custom_gpu_primitive)

CLDNN_DECLARE_PRIMITIVE_TYPE_ID(custom_gpu_primitive);

#ifdef __cplusplus
}
#endif

/// @}
/// @}
/// @}
#endif /* CUSTOM_GPU_PRIMITIVE_H */

