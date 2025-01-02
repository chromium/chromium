// Copyright 2020 rock-chips.com Inc.

#include "onnxruntime_c_api.h"

#ifdef __cplusplus
extern "C" {
#endif

ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_Rknpu,
               _In_ OrtSessionOptions* options);

#ifdef __cplusplus
}
#endif
