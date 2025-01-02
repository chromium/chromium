// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-FileCopyrightText: Copyright 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
// Licensed under the MIT License.

#include "onnxruntime_c_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \param use_arena zero: false. non-zero: true.
 */
ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_ACL, _In_ OrtSessionOptions* options,
               bool enable_fast_math)
ORT_ALL_ARGS_NONNULL;

#ifdef __cplusplus
}
#endif
