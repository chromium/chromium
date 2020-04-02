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

#ifndef GNA_TYPES_XNN_H
#define GNA_TYPES_XNN_H

/******************************************************************************
 *
 * GMM Scoring and Neural Network Accelerator Module
 * API xNN TYPES DEFINITION
 *
 *****************************************************************************/

#include <stdint.h>

#ifdef __cplusplus
extern "C" {  // API uses C linkage so that it can be used by C and C++ applications
#endif

/** Number of input groups constraint - max */
#define XNN_N_GROUP_MAX             8

/** Total number of input elements constraint - must be multiple of */
#define XNN_N_IN_ELEMS_MPLY         8

/** Total number of output elements constraint - must be multiple of */
#define RNN_N_OUT_ELEMS_MPLY        32

/** Total number of input elements constraint - max elements */
#define XNN_N_IN_ELEMS_MAX          UINT16_MAX

/** Number of pwl segments constraint - max  */
#define XNN_N_PWL_SEGS_MAX          128

/** Number of pwl segments constraint - min  */
#define XNN_N_PWL_SEGS_MIN          2

/** Weight elements size constraint - max size B */
#define XNN_W_ELEM_SZ_MAX           2

/** xNN maximum number of Layers  */
#define XNN_LAYERS_MAX_COUNT        1023

/** CNN minimum number of filter coefficients */
#define CNN_N_FLT_COEFF_MIN         48

/** CNN maximum number of filter coefficients */
#define CNN_N_FLT_COEFF_MAX         768

/** CNN number of filter coefficients constraint - must be multiple of */
#define CNN_N_FLT_COEFF_MPLY        4

/** CNN maximum number of filters */
#define CNN_N_FLT_MAX               ((UINT16_MAX + 1) - 4)

/** CNN minimum size of pooling window */
#define CNN_POOL_SIZE_MIN           1

/** CNN maximum size of pooling window */
#define CNN_POOL_SIZE_MAX           6

typedef enum {
    INTEL_AFFINE,                   // cast to intel_affine_layer_t
    INTEL_AFFINE_DIAGONAL,          // also cast to intel_affine_layer_t
    INTEL_RECURRENT,                // cast to intel_recurrent_layer_t
    INTEL_CONVOLUTIONAL,            // cast to intel_convolutional_layer_t
    INTEL_INTERLEAVE,               // no casting, layer details always null
    INTEL_DEINTERLEAVE,             // no casting, layer details always null
    INTEL_COPY,                     // cast to intel_copy_layer_t
    NUM_LAYER_KINDS

} intel_layer_kind_t;

typedef struct
{
    int32_t     bias;
    uint8_t     multiplier;
    char        reserved[3];

} intel_compound_bias_t;

static_assert(8 == sizeof(intel_compound_bias_t), "Invalid size of intel_compound_bias_t");

typedef struct
{
    uint32_t    nBytesPerWeight;
    uint32_t    nBytesPerBias;
    void*       pWeights;
    void*       pBiases;

} intel_affine_func_t;

typedef struct
{
    int32_t     xBase;
    int16_t     yBase;
    int16_t     slope;

} intel_pwl_segment_t;

static_assert(8 == sizeof(intel_pwl_segment_t), "Invalid size of intel_pwl_segment_t");

typedef struct
{
    uint32_t             nSegments; // 0 segments means function is disabled
    intel_pwl_segment_t* pSegments;

} intel_pwl_func_t;

typedef struct
{
    uint32_t    nInputColumns;      // number of input columns
    uint32_t    nInputRows;         // number of input rows
    uint32_t    nOutputColumns;     // number of output columns
    uint32_t    nOutputRows;        // number of output rows
    uint32_t    nBytesPerInput;     // number of bytes per input node
    uint32_t    nBytesPerOutput;    // number of bytes per output node
    uint32_t    nBytesPerIntermediateOutput;
    intel_layer_kind_t nLayerKind;  // layer kind enum
    void*       pLayerStruct;       // layer details
    void*       pInputs;
    void*       pOutputsIntermediate;
    void*       pOutputs;

} intel_nnet_layer_t;

typedef struct
{
    intel_affine_func_t affine;
    intel_pwl_func_t    pwl;        // 0 segments means disabled

} intel_affine_layer_t;

typedef struct
{
    intel_affine_func_t affine;
    intel_pwl_func_t    pwl;            // 0 segments means disabled
    void*               pFeedbackBuffer;// size same as in the output buffer

} intel_recurrent_layer_t;

typedef struct {
    uint32_t nCopyRows;             // number of rows affected (1-8)
    uint32_t nCopyCols;             // number of columns in a row to copy

} intel_copy_layer_t;

typedef enum
{
    INTEL_NO_POOLING    = 0,
    INTEL_MAX_POOLING   = 1,
    INTEL_SUM_POOLING   = 2,
    NUM_POOLING_TYPES

} intel_pool_type_t;

typedef struct
{
    uint32_t    nFilterCoefficients;// including 0-padding if necessary
    uint32_t    nBytesFilterCoefficient;
    uint32_t    nBytesBias;
    uint32_t    nFilters;
    uint32_t    nFeatureMaps;
    uint32_t    nFeatureMapRows;
    uint32_t    nFeatureMapColumns;
    uint32_t    nFilterRows;
    void*       pFilters;           // filters stored one after the other
    void*       pBiases;
    uint32_t    nPoolSize;          // 1 means no pooling
    uint32_t    nPoolStride;

    intel_pool_type_t poolType;     // 0=no pooling, 1=max pooling, 2=sum pooling
    intel_pwl_func_t  pwl;          // 0 segments means disabled

} intel_convolutional_layer_t;

typedef struct
{
    intel_nnet_layer_t *pLayers;    // address of the first layer descriptor
    uint32_t nLayers;               // number of layer descriptors
    uint32_t nGroup;                // grouping level

} intel_nnet_type_t;

#ifdef __cplusplus
}
#endif

#endif  // ifndef GNA_TYPES_XNN_H
