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
 * GMM Scoring and Neural Network Accelerator Module
 * API DEFINITION
 *
 *****************************************************************************/

#ifndef GNAAPI_H
#define GNAAPI_H

#include <stdint.h>

#include "gna-api-status.h"
#include "gna-api-types-gmm.h"
#include "gna-api-types-xnn.h"

#ifdef __cplusplus
extern "C" {  // API uses C linkage so that it can be used by C and C++ applications
#endif

/** Library API import/export macros */
#if 1 == _WIN32
#   if 1 == INTEL_GNA_DLLEXPORT
#       define DLLDECL __declspec(dllexport)
#   else
#       define DLLDECL __declspec(dllimport)
#   endif
#else
#       define DLLDECL
#endif

/**
 * Page Size definition for user space code
 */
#ifndef PAGE_SIZE
#   define PAGE_SIZE 0x1000
#endif // !PAGE_SIZE

/** Maximum padding of page table in bytes */
#define MAXPADFEATVECBYTES  128

/** Maximum number of device handles */
#define MAX_NUM_HANDLES     1

/** NULL handle indicating invalid device */
#define GNA_NULL_HANDLE     0

/** Maximum number of requests that can be submited and waiting in queue  */
#define GNA_QUEUE_LENGTH    64

/** Request id indicating that GNAWait should wait until any request completes */
#define GNA_WAIT_ANY_REQ    0xffffffff

#define MAX_TIMEOUT    180000

/**
 * Pads memory size to given number of Bytes
 *
 * Please always use this padding macro for consistency
 *
 * @memSize size (in bytes) of memory to be padded
 * @align   number of bytes to pad
 * @return  memory size (int bytes) padded to given value
 */
#define ALIGN(memSize, pad)   (((int)((memSize) + pad -1) / pad) * pad)

/**
 * Pads memory size to 64 Bytes
 *
 * Please always use this padding macro for consistency
 *
 * @memSize size (in bytes) of memory to be padded
 * @return  memory size (in bytes) padded to 64 bytes
 */
#define ALIGN64(memSize)        ALIGN(memSize, 64)

/**
 * Verify data sizes used in API and hardware
 *
 * NOTE: If data sizes in application using API differ from data sizes
 *       in API library implementation scoring will not work properly
 */
static_assert(1 == sizeof(int8_t),  "Invalid size of int8_t");
static_assert(2 == sizeof(int16_t), "Invalid size of int16_t");
static_assert(4 == sizeof(int32_t), "Invalid size of int32_t");
static_assert(1 == sizeof(uint8_t), "Invalid size of uint8_t");
static_assert(2 == sizeof(uint16_t),"Invalid size of uint16_t");
static_assert(4 == sizeof(uint32_t),"Invalid size of uint32_t");

/**
* Enumeration of acceleration modes that can be used
* Any enumeration value "bit-wise" AND-ed with GNA_HARDWARE or HW_COMPLIANT flag
* will result in calculating xNN with saturation detection
* Otherwise, fast kernel which does not detect saturation will be used
* Does not apply to GMM kernels, where all versions detect saturation
*/

typedef enum {
    GNA_HARDWARE          = 0xFFFFFFFE, // 0111 ... 1110
    GNA_AUTO              = 0x3, // 0011
    GNA_SOFTWARE          = 0x5, // 0101
    GNA_GENERIC           = 0x7, // 0111
    GNA_SSE4_2            = 0x9, // 1001
    GNA_AVX1              = 0xB, // 1011
    GNA_AVX2              = 0xD, // 1101
} intel_gna_proc_t;

#define HW_COMPLIANT GNA_HARDWARE

static_assert(4 == sizeof(intel_gna_proc_t), "Invalid size of intel_gna_proc_t");

/**
 * // TODO: fill
 */
typedef uint32_t intel_gna_handle_t;

/**
 * intel_gna_status_t members printable descriptions
 *   Size: NUMGNASTATUS + 1
 */
DLLDECL extern const char *GNAStatusName[];

/**
 * intel_gmm_mode_t members printable descriptions
 *   Size: NUMGMMMODES + 1
 */
DLLDECL extern const char *GMMModeName[];

/**
 * // TODO: fill
 */
DLLDECL intel_gna_status_t GNAScoreGaussians(
    intel_gna_handle_t          nGNADevice,
    const intel_feature_type_t* pFeatureType,
    const intel_feature_t*      pFeatureData,
    const intel_gmm_type_t*     pModelType,
    const intel_gmm_t*          pModelData,
    const uint32_t*             pActiveGMMIndices,
    uint32_t                    nActiveGMMIndices,
    uint32_t                    uMaximumScore,
    intel_gmm_mode_t            nGMMMode,
    uint32_t*                   pScores,
    uint32_t*                   pReqId,
    intel_gna_proc_t            nAccelerationType
);

DLLDECL intel_gna_status_t GNAPropagateForward(
	intel_gna_handle_t          nGNADevice,
	const intel_nnet_type_t*    pNeuralNetwork,
	const uint32_t*             pActiveIndices,
	uint32_t                    nActiveIndices,
	uint32_t*                   pReqId,
    intel_gna_proc_t            nAccelerationType
);

// TODO: add output status
/**
 * // TODO: fill
 */
DLLDECL void *GNAAlloc(
    intel_gna_handle_t nGNADevice,   // handle to GNA accelerator
    uint32_t           sizeRequested,
    uint32_t*          sizeGranted
);

/**
 * // TODO: fill
 */
DLLDECL intel_gna_status_t GNAFree(
    intel_gna_handle_t nGNADevice   // handle to GNA accelerator
);

/**
 * // TODO: fill
 */
DLLDECL intel_gna_handle_t GNADeviceOpen(
    intel_gna_status_t* status	        // Status of the call
);

/**
* // TODO: fill
*/
DLLDECL intel_gna_handle_t GNADeviceOpenSetThreads(
    intel_gna_status_t* status,	        // Status of the call
    uint8_t n_threads				// Number of worker threads
);

/**
 * // TODO: fill
 */
DLLDECL intel_gna_status_t GNADeviceClose(
    intel_gna_handle_t nGNADevice // handle to GNA accelerator
);

/**
 * // TODO: fill
 */
DLLDECL intel_gna_status_t GNAWait(
    intel_gna_handle_t nGNADevice,            // handle to GNA accelerator
    uint32_t           nTimeoutMilliseconds,
    uint32_t           reqId                  // IN score request ID
);

#ifdef __cplusplus
}
#endif

#endif  // ifndef GNAAPI_H
