/*******************************************************************************
* Copyright 2018-2019 Intel Corporation
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

// DO NOT EDIT, AUTO-GENERATED

// clang-format off

#ifndef DNNL_DEBUG_H
#define DNNL_DEBUG_H

/// @file
/// Debug capabilities

#include "dnnl_config.h"
#include "dnnl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

const char DNNL_API *dnnl_status2str(dnnl_status_t v);
const char DNNL_API *dnnl_dt2str(dnnl_data_type_t v);
const char DNNL_API *dnnl_fmt_kind2str(dnnl_format_kind_t v);
const char DNNL_API *dnnl_fmt_tag2str(dnnl_format_tag_t v);
const char DNNL_API *dnnl_prop_kind2str(dnnl_prop_kind_t v);
const char DNNL_API *dnnl_prim_kind2str(dnnl_primitive_kind_t v);
const char DNNL_API *dnnl_alg_kind2str(dnnl_alg_kind_t v);
const char DNNL_API *dnnl_rnn_flags2str(dnnl_rnn_flags_t v);
const char DNNL_API *dnnl_rnn_direction2str(dnnl_rnn_direction_t v);
const char DNNL_API *dnnl_engine_kind2str(dnnl_engine_kind_t v);
const char DNNL_API *dnnl_scratchpad_mode2str(dnnl_scratchpad_mode_t v);

const char DNNL_API *dnnl_runtime2str(unsigned v);

/// Forms a format string for a given memory descriptor.
///
/// The format is defined as: 'dt:[p|o|0]:fmt_kind:fmt:extra'.
/// Here:
///  - dt       -- data type
///  - p        -- indicates there is non-trivial padding
///  - o        -- indicates there is non-trivial padding offset
///  - 0        -- indicates there is non-trivial offset0
///  - fmt_kind -- format kind (blocked, wino, etc...)
///  - fmt      -- extended format string (format_kind specific)
///  - extra    -- shows extra fields (underspecified)
int DNNL_API dnnl_md2fmt_str(char *fmt_str, size_t fmt_str_len,
        const dnnl_memory_desc_t *md);

/// Forms a dimension string for a given memory descriptor.
///
/// The format is defined as: 'dim0xdim1x...xdimN
int DNNL_API dnnl_md2dim_str(char *dim_str, size_t dim_str_len,
        const dnnl_memory_desc_t *md);

#ifdef __cplusplus
}
#endif

#endif
