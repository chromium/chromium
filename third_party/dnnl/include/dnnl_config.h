/*******************************************************************************
* Copyright 2019 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef DNNL_CONFIG_H
#define DNNL_CONFIG_H

#include "dnnl_types.h"

/// @cond DO_NOT_DOCUMENT_THIS

// All symbols shall be internal unless marked as DNNL_API
#if defined _WIN32 || defined __CYGWIN__
#define DNNL_HELPER_DLL_IMPORT __declspec(dllimport)
#define DNNL_HELPER_DLL_EXPORT __declspec(dllexport)
#else
#if __GNUC__ >= 4
#define DNNL_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#define DNNL_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#else
#define DNNL_HELPER_DLL_IMPORT
#define DNNL_HELPER_DLL_EXPORT
#endif
#endif

#ifdef DNNL_DLL
#ifdef DNNL_DLL_EXPORTS
#define DNNL_API DNNL_HELPER_DLL_EXPORT
#else
#define DNNL_API DNNL_HELPER_DLL_IMPORT
#endif
#else
#define DNNL_API
#endif

#if defined(__GNUC__)
#define DNNL_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define DNNL_DEPRECATED __declspec(deprecated)
#else
#define DNNL_DEPRECATED
#endif

/// @endcond

// clang-format off

// DNNL CPU threading runtime
#define DNNL_CPU_THREADING_RUNTIME DNNL_RUNTIME_OMP

// DNNL CPU engine runtime
#define DNNL_CPU_RUNTIME DNNL_RUNTIME_OMP

// DNNL GPU engine runtime
#define DNNL_GPU_RUNTIME DNNL_RUNTIME_NONE

// clang-format on

#if defined(DNNL_CPU_RUNTIME) && defined(DNNL_GPU_RUNTIME)
#if (DNNL_CPU_RUNTIME == DNNL_RUNTIME_NONE) \
        || (DNNL_CPU_RUNTIME == DNNL_RUNTIME_OCL)
#error "Unexpected DNNL_CPU_RUNTIME"
#endif
#if (DNNL_GPU_RUNTIME != DNNL_RUNTIME_NONE) \
        && (DNNL_GPU_RUNTIME != DNNL_RUNTIME_OCL)
#error "Unexpected DNNL_GPU_RUNTIME"
#endif
#else
#error "BOTH DNNL_CPU_RUNTIME and DNNL_GPU_RUNTIME must be defined"
#endif

#endif
