// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// The winml "provider factory" is not a true execution provider.
// It is placed here as an execution provider to facilitate the export of the WinMLAdapter API
// via the OrtGetWinMLAdapter method.

#include "onnxruntime_c_api.h"

struct OrtWinApi;
typedef struct OrtWinApi OrtWinApi;

struct WinmlAdapterApi;
typedef struct WinmlAdapterApi WinmlAdapterApi;

ORT_EXPORT const WinmlAdapterApi* ORT_API_CALL OrtGetWinMLAdapter(_In_ uint32_t ort_api_version) NO_EXCEPTION;