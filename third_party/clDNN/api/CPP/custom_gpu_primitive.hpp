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
#include "../C/custom_gpu_primitive.h"
#include "primitive.hpp"
#include "memory.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief This primitive executes a custom kernel provided by the application
/// @details The application is required to provide all relevant details for executing the custom kernel
/// such as: sources, entry point, work sizes and parameter bindings.
    struct custom_gpu_primitive : public primitive_base<custom_gpu_primitive, CLDNN_PRIMITIVE_DESC(custom_gpu_primitive)>
    {
        CLDNN_DECLARE_PRIMITIVE(custom_gpu_primitive)

            /// @brief Constructs custom_gpu_primitive primitive
            /// @param id This primitive id.
            /// @param input Input primitive ids.
            /// @param kernels_code Source code for the kernel
            /// @param kernel_entry_point The name of the entry point function in the kernel
            /// @param kernel_arguments Argument bindings for the entry point function
            /// @param build_options Build options/flags used during the compilation of the custom kernel
            /// @param output_layout Output layout declared by the primitive
            /// @param gws Global work sizes
            /// @param lws Local work sizes
            custom_gpu_primitive(
                const primitive_id& id,
                const std::vector<primitive_id>& input,
                const std::vector<std::string>& kernels_code,
                const std::string& kernel_entry_point,
                const std::vector<cldnn_arg>& kernel_arguments,
                const std::string& build_options,
                const layout& output_layout,
                const std::vector<size_t>& gws = {},
                const std::vector<size_t>& lws = {}
            )
            : primitive_base(id, { input }, output_layout.data_padding)
            , kernels_code(_kernels_code.cpp_ids)
            , kernel_entry_point(kernel_entry_point)
            , kernel_arguments(kernel_arguments)
            , build_options(build_options)
            , output_layout(output_layout)
            , gws(gws.size() ? gws : std::vector<size_t>{ output_layout.count() })
            , lws(lws)
            , _kernels_code(kernels_code)
        {
        }

        /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{custom_gpu_primitive}
        custom_gpu_primitive(const dto* dto)
            : primitive_base(dto)
            , kernels_code(_kernels_code.cpp_ids)
            , kernel_entry_point(dto->kernel_entry_point)
            , kernel_arguments(dto->kernel_arguments, dto->kernel_arguments + dto->kernel_arguments_num)
            , build_options(dto->build_options)
            , output_layout(dto->output_layout)
            , gws(dto->gws, dto->gws + dto->gws_num)
            , lws(dto->lws, dto->lws + dto->lws_num)
            , _kernels_code(dto->kernels_code)
    {
    }

    /// @brief Source code for the kernel
    fixed_size_vector_ref kernels_code;
    /// @brief The name of the entry point function in the kernel
    const std::string kernel_entry_point;
    /// @brief Argument bindings for the entry point function
    const std::vector<cldnn_arg> kernel_arguments;
    /// @brief The kernel's build options
    const std::string build_options;
    /// @brief The output layout declared by the primitive
    const layout output_layout;
    /// @brief The global working sizes
    const std::vector<size_t> gws;
    /// @brief The local working sizes
    const std::vector<size_t> lws;
    

protected:
    primitive_id_arr _kernels_code;

    void update_dto(dto& dto) const override
    {
        dto.kernels_code            = _kernels_code.ref();
        dto.kernel_entry_point      = kernel_entry_point.c_str();
        dto.kernel_arguments        = kernel_arguments.data();
        dto.kernel_arguments_num    = (int)kernel_arguments.size();
        dto.build_options           = build_options.c_str();
        dto.output_layout           = (cldnn_layout)output_layout;
        dto.gws                     = gws.data();
        dto.gws_num                 = (int)gws.size();
        dto.lws                     = lws.data();
        dto.lws_num                 = (int)lws.size();
    }
};
/// @}
/// @}
/// @}
}
