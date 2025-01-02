// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#define ORT_ROCM_CTX

#include "rocm_resource.h"
#include "core/providers/custom_op_context.h"
#include <hip/hip_runtime.h>
#include <miopen/miopen.h>
#include <hipblas/hipblas.h>

namespace Ort {

namespace Custom {

struct RocmContext : public CustomOpContext {
  hipStream_t hip_stream = {};
  miopenHandle_t miopen_handle = {};
  hipblasHandle_t blas_handle = {};

  void Init(const OrtKernelContext& kernel_ctx) {
    const auto& ort_api = Ort::GetApi();
    void* resource = {};
    OrtStatus* status = nullptr;

    status = ort_api.KernelContext_GetResource(
        &kernel_ctx, ORT_ROCM_RESOURCE_VERSION, RocmResource::hip_stream_t, &resource);
    if (status) {
      ORT_CXX_API_THROW("failed to fetch hip stream", OrtErrorCode::ORT_RUNTIME_EXCEPTION);
    }
    hip_stream = reinterpret_cast<hipStream_t>(resource);

    resource = {};
    status = ort_api.KernelContext_GetResource(
        &kernel_ctx, ORT_ROCM_RESOURCE_VERSION, RocmResource::miopen_handle_t, &resource);
    if (status) {
      ORT_CXX_API_THROW("failed to fetch miopen handle", OrtErrorCode::ORT_RUNTIME_EXCEPTION);
    }
    miopen_handle = reinterpret_cast<miopenHandle_t>(resource);

    resource = {};
    status = ort_api.KernelContext_GetResource(
        &kernel_ctx, ORT_ROCM_RESOURCE_VERSION, RocmResource::hipblas_handle_t, &resource);
    if (status) {
      ORT_CXX_API_THROW("failed to fetch hipblas handle", OrtErrorCode::ORT_RUNTIME_EXCEPTION);
    }
    blas_handle = reinterpret_cast<hipblasHandle_t>(resource);
  }
};

}  // namespace Custom
}  // namespace Ort
