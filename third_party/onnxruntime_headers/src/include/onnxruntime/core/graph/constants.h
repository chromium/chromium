// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stddef.h>  // needed for size_t on some platforms

namespace onnxruntime {

constexpr const char* kNoOp = "NoOp";
constexpr const char* kConstant = "Constant";
constexpr const char* kFunctionOp = "_kFunctionOp";
constexpr const char* kConstantValue = "value";
constexpr const char* kOnnxDomain = "";
// NOTE: Node::Init converts kOnnxDomainAlias to kOnnxDomain, so all Node instances use kOnnxDomain.
constexpr const char* kOnnxDomainAlias = "ai.onnx";
constexpr const char* kMLDomain = "ai.onnx.ml";
constexpr const char* kMSDomain = "com.microsoft";
constexpr const char* kPytorchAtenDomain = "org.pytorch.aten";
constexpr const char* kMSExperimentalDomain = "com.microsoft.experimental";
constexpr const char* kMSNchwcDomain = "com.microsoft.nchwc";
constexpr const char* kMSInternalNHWCDomain = "com.ms.internal.nhwc";
constexpr const char* kMSDmlDomain = "com.microsoft.dml";
constexpr const char* kNGraphDomain = "com.intel.ai";
constexpr const char* kMIGraphXDomain = "";
constexpr const char* kVitisAIDomain = "com.xilinx";

// This is moved from the OrtApis::GetAvailableProviders implementation
// where it is enforced
constexpr size_t kMaxExecutionProviderNameLen = 30;

constexpr const char* kCpuExecutionProvider = "CPUExecutionProvider";
constexpr const char* kCudaExecutionProvider = "CUDAExecutionProvider";
constexpr const char* kCudaNHWCExecutionProvider = "CUDANHWCExecutionProvider";
constexpr const char* kDnnlExecutionProvider = "DnnlExecutionProvider";
constexpr const char* kOpenVINOExecutionProvider = "OpenVINOExecutionProvider";
constexpr const char* kVitisAIExecutionProvider = "VitisAIExecutionProvider";
constexpr const char* kTensorrtExecutionProvider = "TensorrtExecutionProvider";
constexpr const char* kNnapiExecutionProvider = "NnapiExecutionProvider";
constexpr const char* kQnnExecutionProvider = "QNNExecutionProvider";
constexpr const char* kRknpuExecutionProvider = "RknpuExecutionProvider";
constexpr const char* kDmlExecutionProvider = "DmlExecutionProvider";
constexpr const char* kMIGraphXExecutionProvider = "MIGraphXExecutionProvider";
constexpr const char* kAclExecutionProvider = "ACLExecutionProvider";
constexpr const char* kArmNNExecutionProvider = "ArmNNExecutionProvider";
constexpr const char* kRocmExecutionProvider = "ROCMExecutionProvider";
constexpr const char* kCoreMLExecutionProvider = "CoreMLExecutionProvider";
constexpr const char* kJsExecutionProvider = "JsExecutionProvider";
constexpr const char* kSnpeExecutionProvider = "SNPEExecutionProvider";
constexpr const char* kTvmExecutionProvider = "TvmExecutionProvider";
constexpr const char* kXnnpackExecutionProvider = "XnnpackExecutionProvider";
constexpr const char* kWebNNExecutionProvider = "WebNNExecutionProvider";
constexpr const char* kWebGpuExecutionProvider = "WebGpuExecutionProvider";
constexpr const char* kCannExecutionProvider = "CANNExecutionProvider";
constexpr const char* kAzureExecutionProvider = "AzureExecutionProvider";
constexpr const char* kVSINPUExecutionProvider = "VSINPUExecutionProvider";

constexpr const char* kExecutionProviderSharedLibraryPath = "shared_lib_path";
constexpr const char* kExecutionProviderSharedLibraryEntry = "provider_factory_entry_point";

}  // namespace onnxruntime
