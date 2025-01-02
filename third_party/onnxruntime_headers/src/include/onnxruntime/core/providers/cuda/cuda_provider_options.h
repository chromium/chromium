// Copyright (c) Microsoft Corporation. All rights reserved.
// Copyright (c) 2023 NVIDIA Corporation.
// Licensed under the MIT License.

#pragma once

#include <limits>

#include "onnxruntime_c_api.h"
#include "core/framework/arena_extend_strategy.h"

/// <summary>
/// Options for the CUDA provider that are passed to SessionOptionsAppendExecutionProvider_CUDA_V2.
/// Please note that this struct is *similar* to OrtCUDAProviderOptions but only to be used internally.
/// Going forward, new cuda provider options are to be supported via this struct and usage of the publicly defined
/// OrtCUDAProviderOptions will be deprecated over time.
/// User can only get the instance of OrtCUDAProviderOptionsV2 via CreateCUDAProviderOptions.
/// </summary>
struct OrtCUDAProviderOptionsV2 {
  int device_id = 0;                                                                                           // cuda device id.
  int has_user_compute_stream = 0;                                                                             // indicator of user specified CUDA compute stream.
  void* user_compute_stream = nullptr;                                                                         // user specified CUDA compute stream.
  int do_copy_in_default_stream = 1;                                                                           // flag specifying if the default stream is to be used for copying.
  OrtCudnnConvAlgoSearch cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;                            // cudnn algo search enum.
  size_t gpu_mem_limit = std::numeric_limits<size_t>::max();                                                   // BFC Arena memory limit for CUDA.
                                                                                                               // (will be overridden by contents of `default_memory_arena_cfg` is it exists)
  onnxruntime::ArenaExtendStrategy arena_extend_strategy = onnxruntime::ArenaExtendStrategy::kNextPowerOfTwo;  // BFC Arena extension strategy.
                                                                                                               // (will be overridden by contents of `default_memory_arena_cfg` is it exists)
  OrtArenaCfg* default_memory_arena_cfg = nullptr;                                                             // BFC Arena config flags.
  int cudnn_conv_use_max_workspace = 1;                                                                        // flag specifying if maximum workspace can be used in cudnn conv algo search.
  int enable_cuda_graph = 0;                                                                                   // flag specifying if the CUDA graph is to be captured for the model.
  int cudnn_conv1d_pad_to_nc1d = 0;                                                                            // flag specifying if pad Conv1D's input [N,C,D] to [N,C,1,D] or [N,C,D,1].
  int tunable_op_enable = 0;                                                                                   // flag specifying if TunableOp is enabled.
  int tunable_op_tuning_enable = 0;                                                                            // flag specifying if TunableOp is enabled for tuning, this relies on TunableOp is enabled.
  int tunable_op_max_tuning_duration_ms = 0;                                                                   // Max tuning duration time limit for TunableOp.
  int enable_skip_layer_norm_strict_mode = 0;                                                                  // flag specifying if SkipLayerNorm is in strict mode. If true, use LayerNormalization kernel.
                                                                                                               // The strict mode has better accuracy but lower performance.
  int prefer_nhwc = 0;                                                                                         // make the CUDA EP NHWC preferred
  int use_ep_level_unified_stream = 0;                                                                         // flag specifying if ep level stream is used or not
  int use_tf32 = 1;                                                                                            // use TF32
  int fuse_conv_bias = 0;                                                                                      // Enable CUDNN Frontend kernel fusing, results in JIT compiles
  int sdpa_kernel = 0;                                                                                         // Scaled Dot Product Attention kernel option
};
