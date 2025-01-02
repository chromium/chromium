// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// This header is to expose a context for cuda custom ops.
// By the context, a custom cuda operator could fetch existing resources,
// such as cuda stream and cudnn handle, for reusing.

// For concrete usage, pls find page here:
// https://onnxruntime.ai/docs/reference/operators/add-custom-op.html#custom-ops-for-cuda-and-rocm

#pragma once

#define ORT_CUDA_CTX

#include <cuda.h>
#include <cuda_runtime.h>
#ifndef USE_CUDA_MINIMAL
#include <cublas_v2.h>
#include <cudnn.h>
#endif

#include "core/providers/cuda/cuda_resource.h"
#include "core/providers/custom_op_context.h"

namespace Ort {

namespace Custom {

struct CudaContext : public CustomOpContext {
  cudaStream_t cuda_stream = {};
  cudnnHandle_t cudnn_handle = {};
  cublasHandle_t cublas_handle = {};
  OrtAllocator* deferred_cpu_allocator = {};
  // below are cuda ep options
  int16_t device_id = 0;
  int32_t arena_extend_strategy = 0;
  int32_t cudnn_conv_algo_search = 0;
  bool cudnn_conv_use_max_workspace = true;
  bool cudnn_conv1d_pad_to_nc1d = false;
  bool enable_skip_layer_norm_strict_mode = false;
  bool prefer_nhwc = false;
  bool use_tf32 = true;
  bool fuse_conv_bias = true;

  void Init(const OrtKernelContext& kernel_ctx) {
    cuda_stream = FetchResource<cudaStream_t>(kernel_ctx, CudaResource::cuda_stream_t);
    cudnn_handle = FetchResource<cudnnHandle_t>(kernel_ctx, CudaResource::cudnn_handle_t);
    cublas_handle = FetchResource<cublasHandle_t>(kernel_ctx, CudaResource::cublas_handle_t);
    deferred_cpu_allocator = FetchResource<OrtAllocator*>(kernel_ctx, CudaResource::deferred_cpu_allocator_t);

    device_id = FetchResource<int16_t>(kernel_ctx, CudaResource::device_id_t);
    arena_extend_strategy = FetchResource<int32_t>(kernel_ctx, CudaResource::arena_extend_strategy_t);
    cudnn_conv_algo_search = FetchResource<int32_t>(kernel_ctx, CudaResource::cudnn_conv_algo_search_t);
    cudnn_conv_use_max_workspace = FetchResource<bool>(kernel_ctx, CudaResource::cudnn_conv_use_max_workspace_t);

    cudnn_conv1d_pad_to_nc1d = FetchResource<bool>(kernel_ctx, CudaResource::cudnn_conv1d_pad_to_nc1d_t);
    enable_skip_layer_norm_strict_mode = FetchResource<bool>(
        kernel_ctx, CudaResource::enable_skip_layer_norm_strict_mode_t);
    prefer_nhwc = FetchResource<bool>(kernel_ctx, CudaResource::prefer_nhwc_t);
    use_tf32 = FetchResource<bool>(kernel_ctx, CudaResource::use_tf32_t);
    fuse_conv_bias = FetchResource<bool>(kernel_ctx, CudaResource::fuse_conv_bias_t);
  }

  template <typename T>
  T FetchResource(const OrtKernelContext& kernel_ctx, CudaResource resource_type) {
    if constexpr (sizeof(T) > sizeof(void*)) {
      ORT_CXX_API_THROW("void* is not large enough to hold resource type: " + std::to_string(resource_type),
                        OrtErrorCode::ORT_INVALID_ARGUMENT);
    }
    const auto& ort_api = Ort::GetApi();
    void* resource = {};
    OrtStatus* status = ort_api.KernelContext_GetResource(
        &kernel_ctx, ORT_CUDA_RESOURCE_VERSION, resource_type, &resource);
    if (status) {
      ORT_CXX_API_THROW("Failed to fetch cuda ep resource, resource type: " + std::to_string(resource_type),
                        OrtErrorCode::ORT_RUNTIME_EXCEPTION);
    }
    T t = {};
    memcpy(&t, &resource, sizeof(T));
    return t;
  }

  void* AllocDeferredCpuMem(size_t size) const {
    if (0 == size) {
      return {};
    }
    const auto& ort_api = Ort::GetApi();
    void* mem = {};
    auto status = ort_api.AllocatorAlloc(deferred_cpu_allocator, size, &mem);
    if (status) {
      ORT_CXX_API_THROW("failed to allocate deferred cpu memory", OrtErrorCode::ORT_RUNTIME_EXCEPTION);
    }
    return mem;
  }

  void FreeDeferredCpuMem(void* mem) const {
    if (mem) {
      const auto& ort_api = Ort::GetApi();
      auto status = ort_api.AllocatorFree(deferred_cpu_allocator, mem);
      if (status) {
        ORT_CXX_API_THROW("failed to free deferred cpu memory", OrtErrorCode::ORT_RUNTIME_EXCEPTION);
      }
    }
  }
};

}  // namespace Custom
}  // namespace Ort
