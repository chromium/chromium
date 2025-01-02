// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Dummy file to provide a signal in the ONNX Runtime C cocoapod as to whether the WebGPU EP was included in the build.
// If it was, this file will be included in the cocoapod, and a test like this can be used:
//
//   #if __has_include(<onnxruntime/webgpu_provider_factory.h>)
//     #define WEBGPU_EP_AVAILABLE 1
//   #else
//     #define WEBGPU_EP_AVAILABLE 0
//   #endif

// The WebGPU EP can be enabled via the generic SessionOptionsAppendExecutionProvider method, so no direct usage of
// the provider factory is required.
