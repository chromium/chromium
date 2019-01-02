/*******************************************************************************
* Copyright 2018 Intel Corporation
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

/* DO NOT EDIT, AUTO-GENERATED */

#ifndef MKLDNN_DEBUG_H
#define MKLDNN_DEBUG_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* All symbols shall be internal unless marked as MKLDNN_API */
#if defined _WIN32 || defined __CYGWIN__
#   define MKLDNN_HELPER_DLL_IMPORT __declspec(dllimport)
#   define MKLDNN_HELPER_DLL_EXPORT __declspec(dllexport)
#else
#   if __GNUC__ >= 4
#       define MKLDNN_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
#       define MKLDNN_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
#   else
#       define MKLDNN_HELPER_DLL_IMPORT
#       define MKLDNN_HELPER_DLL_EXPORT
#   endif
#endif

#ifdef MKLDNN_DLL
#   ifdef MKLDNN_DLL_EXPORTS
#       define MKLDNN_API MKLDNN_HELPER_DLL_EXPORT
#   else
#       define MKLDNN_API MKLDNN_HELPER_DLL_IMPORT
#   endif
#else
#   define MKLDNN_API
#endif

#if defined (__GNUC__)
#   define MKLDNN_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#   define MKLDNN_DEPRECATED __declspec(deprecated)
#else
#   define MKLDNN_DEPRECATED
#endif

#include "mkldnn_types.h"
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#ifdef __cplusplus
extern "C" {
#endif

const char MKLDNN_API *mkldnn_status2str(mkldnn_status_t v);
const char MKLDNN_API *mkldnn_dt2str(mkldnn_data_type_t v);
const char MKLDNN_API *mkldnn_rmode2str(mkldnn_round_mode_t v);
const char MKLDNN_API *mkldnn_fmt2str(mkldnn_memory_format_t v);
const char MKLDNN_API *mkldnn_prop_kind2str(mkldnn_prop_kind_t v);
const char MKLDNN_API *mkldnn_prim_kind2str(mkldnn_primitive_kind_t v);
const char MKLDNN_API *mkldnn_alg_kind2str(mkldnn_alg_kind_t v);
const char MKLDNN_API *mkldnn_rnn_direction2str(mkldnn_rnn_direction_t v);

#ifdef __cplusplus
}
#endif

#endif
