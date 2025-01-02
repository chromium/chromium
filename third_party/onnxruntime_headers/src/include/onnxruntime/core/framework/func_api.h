#pragma once
#include "core/common/status.h"
using onnxruntime::common::Status;  // TODO: Needed by WinML, but shouldn't be put into the global namespace like this

namespace onnxruntime {

// AllocateFunc(void* handle, size_t alignment, size_t size)
using AllocateFunc = void* (*)(void*, size_t, size_t);
using DestroyFunc = void (*)(void*, void*);
using AllocatorHandle = void*;

typedef struct {
  // right now we only include allocation for host memory
  AllocateFunc allocate_func;
  DestroyFunc release_func;
  AllocatorHandle allocator_handle;
  const char* node_name;
} ComputeContext;

using FunctionState = void*;
// take the ComputeContext, and create a function state.
using CreateFunctionStateC = int (*)(ComputeContext*, FunctionState*);
// pass in the function state and input/output tensors, perform compute and return status
using ComputeFuncC = common::Status (*)(FunctionState, const OrtApi*, OrtKernelContext*);
// release the function state.
using DestroyFunctionStateC = void (*)(FunctionState);
}  // namespace onnxruntime
