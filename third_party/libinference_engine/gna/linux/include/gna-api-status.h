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

#ifndef _INTEL_GNA_STATUS_H
#define _INTEL_GNA_STATUS_H

#ifdef __cplusplus
extern "C" {  // API uses C linkage so that it can be used by C and C++ applications
#endif

/**
 * Status type for GNA API software and GNA hardware device
 */
typedef enum _intel_gna_status_t
{
    GNA_NOERROR,             //00 Success: Operation successful, no errors or warnings
    GNA_DEVICEBUSY,          //01 Warning: Device busy - accelerator is still running, can not enqueue more requests
    GNA_SSATURATE,           //02 Warning: Scoring saturation - an arithmetic operation has resulted in saturation
    GNA_ERR_QUEUE,           //03 Error: Queue can not create or enqueue more requests
    GNA_READFAULT,           //04 Error: Scoring data: invalid input
    GNA_WRITEFAULT,          //05 Error: Scoring data: invalid output buffer
    GNA_BADFEATWIDTH,        //06 Error: Feature vector: width not supported
    GNA_BADFEATLENGTH,       //07 Error: Feature vector: length not supported
    GNA_BADFEATOFFSET,       //08 Error: Feature vector: offset not supported
    GNA_BADFEATALIGN,        //09 Error: Feature vector: invalid memory alignment

    GNA_BADFEATNUM,          //10 Error: Feature vector: Number of feature vectors not supported
    GNA_INVALIDINDICES,      //11 Error: Scoring data: number of active indices  not supported
    GNA_DEVNOTFOUND,         //12 Error: Device: not available
    GNA_OPENFAILURE,         //13 Error: Device: failed to open
    GNA_INVALIDHANDLE,       //14 Error: Device: invalid handle
    GNA_CPUTYPENOTSUPPORTED, //15 Error: Device: processor type not supported
    GNA_PARAMETEROUTOFRANGE, //16 Error: Device: GMM Parameter out of Range error occurred
    GNA_VAOUTOFRANGE,        //17 Error: Device: Virtual Address out of range on DMA ch.
    GNA_UNEXPCOMPL,          //18 Error: Device: Unexpected completion during PCIe operation
    GNA_DMAREQERR,           //19 Error: Device: DMA error during PCIe operation

    GNA_MMUREQERR,           //20 Error: Device: MMU error during PCIe operation
    GNA_BREAKPOINTPAUSE,     //21 Error: Device: GMM accelerator paused on breakpoint
    GNA_BADMEMALIGN,         //22 Error: Device: invalid memory alignment
    GNA_INVALIDMEMSIZE,      //23 Error: Device: requested memory size not supported
    GNA_MODELSIZEEXCEEDED,   //24 Error: Device: request's model configuration exceeded supported GNA_HW mode limits
    GNA_BADREQID,            //25 Error: Device: invalid scoring request identifier
    GNA_WAITFAULT,           //26 Error: Device: wait failed
    GNA_IOCTLRESERR,         //27 Error: Device: IOCTL result retrieval failed
    GNA_IOCTLSENDERR,        //28 Error: Device: sending IOCTL failed
    GNA_NULLARGNOTALLOWED,   //29 Error: NULL argument not allowed

    GNA_NULLARGREQUIRED,     //30 Error: NULL argument is required
    GNA_ERR_UNKNOWN,         //31 Error: Unknown internal error occurred
    GNA_ERR_MEM_ALLOC1,      //32 Error: Memory: Already allocated, only single allocation per device is allowed
    GNA_ERR_RESOURCES,       //33 Error: Unable to create new resources
    GNA_ERR_NOT_MULTIPLY,    //34 Error: Value is not multiply of required value
    GNA_ERR_DEV_FAILURE,     //35 Error: Critical device error occurred, device has been reset
    GMM_BADMEANWIDTH,        //36 Error: Mean vector: width not supported
    GMM_BADMEANOFFSET,       //37 Error: Mean vector: offset not supported
    GMM_BADMEANSETOFF,       //38 Error: Mean vector: set offset not supported
    GMM_BADMEANALIGN,        //39 Error: Mean vector: invalid memory alignment

    GMM_BADVARWIDTH,         //40 Error: Variance vector: width not supported
    GMM_BADVAROFFSET,        //41 Error: Variance vector: offset not supported
    GMM_BADVARSETOFF,        //42 Error: Variance vector: set offset not supported
    GMM_BADVARSALIGN,        //43 Error: Variance vector: invalid memory alignment
    GMM_BADGCONSTOFFSET,     //44 Error: Gconst: set offset not supported
    GMM_BADGCONSTALIGN,      //45 Error: Gconst: invalid memory alignment
    GMM_BADMIXCNUM,          //46 Error: Scoring data: number of mixture components not supported
    GMM_BADNUMGMM,           //47 Error: Scoring data: number of GMMs not supported
    GMM_BADMODE,             //48 Error: Scoring data: GMM scoring mode not supported
    XNN_ERR_NET_LYR_NO,      //49 Error: XNN: Not supported number of layers

    XNN_ERR_LYR_TYPE,        //50 Error: XNN: Not supported layer type
    XNN_ERR_LYR_CFG,         //51 Error: XNN: Invalid layer configuration
    XNN_ERR_NO_FEEDBACK,     //52 Error: XNN: No RNN feedback buffer specified
    XNN_ERR_NO_LAYERS,       //53 Error: XNN: At least one layer must be specified
    XNN_ERR_GROUPING,        //54 Error: XNN: Invalid grouping factor
    XNN_ERR_INPUT_BYTES,     //55 Error: XNN: Invalid number of bytes per input
    XNN_ERR_INT_OUTPUT_BYTES,//56 Error: XNN: Invalid number of bytes per intermediate output
    XNN_ERR_OUTPUT_BYTES,    //57 Error: XNN: Invalid number of bytes per output
    XNN_ERR_WEIGHT_BYTES,    //58 Error: XNN: Invalid number of bytes per weight
    XNN_ERR_BIAS_BYTES,      //59 Error: XNN: Invalid number of bytes per bias
    XNN_ERR_BIAS_MULTIPLIER, //60 Error: XNN: Multiplier larger than 255
    XNN_ERR_PWL_SEGMENTS,    //61 Error: XNN: Activation function segment count invalid, valid values: <2, 128>
    XNN_ERR_PWL_DATA,        //62 Error: XNN: Activation function enabled but segment data not set

    XNN_ERR_MM_INVALID_IN,   //63 Error: XNN: Invalid input data or configuration in matrix mul. op.

    NUMGNASTATUS
} intel_gna_status_t;       // Status type for GNA API software and GNA hardware device

static_assert(4 == sizeof(intel_gna_status_t), "Invalid size of intel_gna_status_t");

#ifdef __cplusplus
}
#endif

#endif //infndef _INTEL_GNA_STATUS_H
