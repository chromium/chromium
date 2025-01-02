// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "onnxruntime_c_api.h"

// NNAPIFlags are bool options we want to set for NNAPI EP
// This enum is defined as bit flags, and cannot have negative value
// To generate an uint32_t nnapi_flags for using with OrtSessionOptionsAppendExecutionProvider_Nnapi below,
//   uint32_t nnapi_flags = 0;
//   nnapi_flags |= NNAPI_FLAG_USE_FP16;
enum NNAPIFlags {
  NNAPI_FLAG_USE_NONE = 0x000,

  // Using fp16 relaxation in NNAPI EP, this may improve perf but may also reduce precision
  NNAPI_FLAG_USE_FP16 = 0x001,

  // Use NCHW layout in NNAPI EP, this is only available after Android API level 29
  // Please note for now, NNAPI perform worse using NCHW compare to using NHWC
  NNAPI_FLAG_USE_NCHW = 0x002,

  // Prevent NNAPI from using CPU devices.
  //
  // NNAPI is more efficient using GPU or NPU for execution, and NNAPI might fall back to its own CPU implementation
  // for operations not supported by GPU/NPU. The CPU implementation of NNAPI (which is called nnapi-reference)
  // might be less efficient than the optimized versions of the operation of ORT. It might be advantageous to disable
  // the NNAPI CPU fallback and handle execution using ORT kernels.
  //
  // For some models, if NNAPI would use CPU to execute an operation, and this flag is set, the execution of the
  // model may fall back to ORT kernels.
  //
  // This option is only available after Android API level 29, and will be ignored for Android API level 28-
  //
  // For NNAPI device assignments, see https://developer.android.com/ndk/guides/neuralnetworks#device-assignment
  // For NNAPI CPU fallback, see https://developer.android.com/ndk/guides/neuralnetworks#cpu-fallback
  //
  // Please note, the NNAPI EP will return error status if both NNAPI_FLAG_CPU_DISABLED
  // and NNAPI_FLAG_CPU_ONLY flags are set
  NNAPI_FLAG_CPU_DISABLED = 0x004,

  // Using CPU only in NNAPI EP, this may decrease the perf but will provide
  // reference output value without precision loss, which is useful for validation
  //
  // Please note, the NNAPI EP will return error status if both NNAPI_FLAG_CPU_DISABLED
  // and NNAPI_FLAG_CPU_ONLY flags are set
  NNAPI_FLAG_CPU_ONLY = 0x008,

  // Keep NNAPI_FLAG_LAST at the end of the enum definition
  // And assign the last NNAPIFlag to it
  NNAPI_FLAG_LAST = NNAPI_FLAG_CPU_ONLY,
};

#ifdef __cplusplus
extern "C" {
#endif

ORT_EXPORT ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_Nnapi,
                          _In_ OrtSessionOptions* options, uint32_t nnapi_flags);

#ifdef __cplusplus
}
#endif
