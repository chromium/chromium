// Copyright (c) Microsoft Corporation. All rights reserved.
// Copyright (c) Huawei. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>

#include "onnxruntime_c_api.h"
#include "core/framework/arena_extend_strategy.h"

struct OrtCANNProviderOptions {
  int device_id;                                           // CANN device id
  size_t npu_mem_limit;                                    // BFC Arena memory limit for CANN
  onnxruntime::ArenaExtendStrategy arena_extend_strategy;  // Strategy used to grow the memory arena
  int enable_cann_graph;                                   // Flag indicating if prioritizing the use of
                                                           // CANN's graph-running capabilities
  int dump_graphs;                                         // Flag indicating if dumping graphs
  int dump_om_model;                                       // Flag indicating if dumping om model
  std::string precision_mode;                              // Operator Precision Mode
  std::string op_select_impl_mode;                         // Operator-level model compilation options:
                                                           // Mode selection
  std::string optypelist_for_implmode;                     // Operator-level model compilation options:
                                                           // Operator list
  OrtArenaCfg* default_memory_arena_cfg;                   // CANN memory arena configuration parameters
};
