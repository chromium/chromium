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

#ifndef GNA_TYPES_GMM_H
#define GNA_TYPES_GMM_H

/******************************************************************************
 *
 * GMM Scoring and Neural Network Accelerator Module
 * API GMM TYPES DEFINITION
 *
 *****************************************************************************/

#include <stdint.h>

#ifdef __cplusplus
extern "C" {  // API uses C linkage so that it can be used by C and C++ applications
#endif

/** Maximum number of mixture components per GMM State */
#define GMMMAXMIXCOMP   4096

/** Maximum number of GMM states, active list elements and  */
#define GMMMAXMODELS    262144

/** Size of memory alignment for mean, variance vectors and Gaussian Constants */
#define GMMVMEMALIGN    8

/** Size of memory alignment for feature vectors */
#define GMMFVMEMALIGN   64

/** Feature vector width in bytes */
#define GMMFVWIDTHB     1

/** Maximum number of feature vectors */
#define GMMMAXVECTORS   8

/** Minimum length of vector */
#define GMMMINVECLEN    24

/** Maximum length of vector */
#define GMMMAXVECLEN    96

/** Allowed align of vector lengths */
#define GMMFVLENALIGN   8

/** Mean vector width in bytes */
#define GMMMVWIDTHB     1

/** Minimum Mean Vector Set Offset in bytes */
#define GMMMINMEANSETOFFBYTES   24

/** Maximum Mean Vector Set Offset in bytes */
#define GMMMAXMEANSETOFFBYTES   GMMMAXMIXCOMP*GMMMAXVECLEN*GMMMVWIDTHB

/** Minimum variance vector width in bytes */
#define GMMMINVVWIDTHB  1

/** Maximum variance vector width in bytes */
#define GMMMAXVVWIDTHB  2

/** Minimum variance Vector Set Offset in bytes */
#define GMMMINVARSETOFFBYTES    24

/** Maximum Variance Vector Set Offset in bytes */
#define GMMMAXVARSETOFFBYTES    GMMMAXMIXCOMP*GMMMAXVECLEN*GMMMAXVVWIDTHB

/** Gaussian Constants width in bytes */
#define GMMGCVWIDTHB    4

/** Maximum Gaussian Constants Set Offset in bytes */
#define GMMMAXSCONSTETOFFBYTES  GMMMAXMIXCOMP*GMMGCVWIDTHB

/** Score width in bytes */
#define GMMSCOREWIDTHB  4

typedef struct
{
	uint32_t nFeatBytesPerElement;  // number of bytes per element
	uint32_t nFeatOffsetBytes;      // # of bytes from one feature vec to next
	uint32_t nLength;               // number of elements per vector

} intel_feature_type_t;

typedef struct
{
	const void* pVector;            // address of first feature vector
	uint32_t    nVectors;           // number of feature vectors

} intel_feature_t;

typedef enum
{
	MAXMIX8,
	MAXMIX16,
	LOGSUM,
	NUMGMMMODES

} intel_gmm_mode_t;

typedef struct
{
    uint32_t nMeanOffsetBytes;      // # of bytes from one mean vec to next
    uint32_t nMeanSetOffsetBytes;   // # of bytes from one mean set to next
    uint32_t nMeanBytesPerElement;  // # of bytes per element
    uint32_t nVarOffsetBytes;       // # of bytes from one var vec to next
    uint32_t nVarSetOffsetBytes;    // # of bytes from one var set to next
    uint32_t nVarBytesPerElement;   // # of bytes per element in var vector
    uint32_t nGConstSetOffsetBytes; // # of bytes from one const set to next
    uint32_t nLength;               // # of elements per vector

} intel_gmm_type_t;

typedef struct
{
    const void* pMeans;             // addr of 1st mean vector of 1st GMM
    const void* pVars;              // addr of 1st var vector of 1st GMM
    const void* pGaussConsts;       // addr of 1st const term of 1st GMM
    uint32_t    nMixComponents;     // number of mixture components per GMM
    uint32_t    nGMMs;              // number of GMMs

} intel_gmm_t;

#ifdef __cplusplus
}
#endif

#endif  // ifndef GNA_TYPES_GMM_H
