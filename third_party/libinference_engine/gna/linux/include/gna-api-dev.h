//*****************************************************************************
// Copyright (C) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.
//
//
// SPDX-License-Identifier: Apache-2.0
//*****************************************************************************

/******************************************************************************
 *
 * GNA API Header for internal development purposes
 *
 *****************************************************************************/

#ifndef GNA_API_DEV_H
#define GNA_API_DEV_H

#include "gna-api.h"
#include "profiler.h"

#ifdef __cplusplus
extern "C" {  // API uses C linkage so that it can be used by C and C++ applications
#endif

typedef enum {
    GNA_HW                = 0xFFFFFFFE, // 0111 ... 1110
    GNA_AUTO_SAT          = 0x2, // 0010
    GNA_AUTO_FAST         = 0x3, // 0011
    GNA_SW_SAT            = 0x4, // 0100
    GNA_SW_FAST           = 0x5, // 0101
    GNA_GEN_SAT           = 0x6, // 0110
    GNA_GEN_FAST          = 0x7, // 0111
    GNA_SSE4_2_SAT        = 0x8, // 1000
    GNA_SSE4_2_FAST       = 0x9, // 1001
    GNA_AVX1_SAT          = 0xA, // 1010
    GNA_AVX1_FAST         = 0xB, // 1011
    GNA_AVX2_SAT          = 0xC, // 1100
    GNA_AVX2_FAST         = 0xD, // 1101

    NUM_GNA_ACCEL_MODES,

    // DLL internal modes, do not use from API
    GNA_CNL_SAT,
    GNA_CNL_FAST
} gna_acc_t;

static_assert(4 == sizeof(gna_acc_t), "Invalid size of gna_acc_t");

DLLDECL void gmmSetThreads(
    int nthreads);

/**
 * GNA Wait with getting performance results
 */
DLLDECL intel_gna_status_t GNAWaitPerfRes(
    intel_gna_handle_t  nGNADevice,         // handle to GNA accelerator
    uint32_t            nTimeoutMilliseconds,
    uint32_t            reqId,              // IN score request ID
    intel_gna_perf_t*   perfResults         //  results buffer to save measurements to
);


#ifdef __cplusplus
}
#endif

#endif // GNA_API_DEV_H
