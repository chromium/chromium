// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/providers/resource.h"

#define ORT_CUDA_RESOURCE_VERSION 3

enum CudaResource : int {
  cuda_stream_t = cuda_resource_offset,  // 10000
  cudnn_handle_t,
  cublas_handle_t,
  deferred_cpu_allocator_t,
  // below are cuda ep options
  device_id_t,  // 10004
  arena_extend_strategy_t,
  cudnn_conv_algo_search_t,
  cudnn_conv_use_max_workspace_t,
  cudnn_conv1d_pad_to_nc1d_t,
  enable_skip_layer_norm_strict_mode_t,
  prefer_nhwc_t,
  use_tf32_t,
  fuse_conv_bias_t
};
