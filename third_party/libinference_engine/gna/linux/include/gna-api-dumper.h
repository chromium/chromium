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
 * DUMPER API DEFINITION
 *
 *****************************************************************************/

#ifndef GNAAPIDUMPER_H
#define GNAAPIDUMPER_H


#include "gna-api.h"

#ifdef __cplusplus
extern "C" {  // API uses C linkage so that it can be used by C and C++ applications
#endif


/**
 * Header describing parameters of dumped model.
 * Structured is partially filled by GNADumpXnn with parameters necessary for SueScrek,
 * other fields are populated by user as necessary, other fields are populated by user.
 */
typedef struct _intel_gna_model_header
{
    uint32_t layer_descriptor_base; // Offset in bytes of first layer descriptor in network.
    uint32_t model_size;            // Total size of model in bytes determined by GNADumpXnn including hw descriptors, model data and input/output buffers.
    uint32_t gna_mode;              // Mode of GNA operation, 1 = XNN mode (default), 0 = GMM mode.
    uint32_t layer_count;           // Number of layers in model.

    uint32_t bytes_per_input;       // Network Input resolution in bytes.
    uint32_t bytes_per_output;      // Network Output resolution in bytes.
    uint32_t input_nodes_count;     // Number of network input nodes.
    uint32_t output_nodes_count;    // Number of network output nodes.

    uint32_t input_descriptor_offset;// Offset in bytes of input pointer descriptor field that need to be set for processing.
    uint32_t output_descriptor_offset;// Offset in bytes of output pointer descriptor field that need to be set for processing.

    uint32_t rw_region_size;        // Size in bytes of read-write region of statically linked GNA model.
    float    input_scaling_factor;   // Scaling factor used for quantization of input values.
    float    output_scaling_factor;  // Scaling factor used for quantization of output values.

    uint8_t  reserved[12];          // Padding to 64B.
} intel_gna_model_header;

static_assert(64 == sizeof(intel_gna_model_header), "Invalid size of intel_gna_model_header");

typedef void* (*intel_gna_alloc_cb)(size_t size);

/**
 * Converts neural network to hardware consumable format ready for processing,
 * allocates memory and stores converted model within it.
 * NOTE:
 * - User should call free() to release returned memory buffer when no longer necessary.
 *
 * @param neuralNetwork Model network topology.
 * @param activeIndices Pointer to active indices array if active list is used or NULL.
 * @param activeIndicesCount Number of active indices in active indices array if active list is used or 0.
 * @param modelHeader   (out) Header describing parameters of model being dumped.
 * @param status        (out) Status of conversion and dumping.
 * @param customAlloc   Pointer to a function with custom memory allocation. Total model size needs to be passed as parameter.
 * @return Pointer to allocated memory buffer with dumped model.
 */
DLLDECL void* GNADumpXnn(
    const intel_nnet_type_t*    neuralNetwork,
    const uint32_t*             activeIndices,
    uint32_t                    activeIndicesCount,
    intel_gna_model_header*     modelHeader,
    intel_gna_status_t*         status,
    intel_gna_alloc_cb          customAlloc);


#ifdef __cplusplus
}
#endif

#endif  // ifndef GNAAPIDUMPER_H
