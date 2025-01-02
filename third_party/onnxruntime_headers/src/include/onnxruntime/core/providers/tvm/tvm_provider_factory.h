// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef TVM_EXECUTION_PROVIDER_FACTORY_H
#define TVM_EXECUTION_PROVIDER_FACTORY_H

#include "onnxruntime_c_api.h"

#ifdef __cplusplus
extern "C" {
#endif

ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_Tvm, _In_ OrtSessionOptions* options, _In_ const char* opt_str);

#ifdef __cplusplus
}
#endif

#endif  // TVM_EXECUTION_PROVIDER_FACTORY_H
