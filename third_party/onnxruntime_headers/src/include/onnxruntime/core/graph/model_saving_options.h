// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

namespace onnxruntime {

class PrepackedWeightsForGraph;

// These options affect how the model initializers are written to the external file.
// This includes options to align external initializer offset.
// For models running on CPU, ORT will try to use mmap to load external
// initializers. To use mmap, external initializer need to be offset aligned.
// ORT saves external initializers into single data file, each initializer is
// accessed with offset(start position of initializer) and length(byte length of
// initializer) of the data file. To use mmap, each offset need to be aligned
// which means offset need to divisible by allocation granularity(64KB for
// windows and 4K for other OSes). With align_offset to true, ORT will align
// offset for large initializer when save ONNX model with external data file.
struct ModelSavingOptions {
  explicit ModelSavingOptions(size_t size_threshold)
      : initializer_size_threshold(size_threshold) {}

  // Mimimal initializer size in bytes to be externalized on disk
  size_t initializer_size_threshold;
  // Offset will always be page aligned and allocation granularity aligned for
  // mmap support. This is done by padding previous tensor data with zeros
  // keeping same length.
  bool align_offset = false;
  // Alignment threshold for size of data.
  // Having a low threshold will waste file space for small initializers.
  // Only when tensor's data size is > the page_align_threshold it will be force
  // aligned. Default to 1MB.
  int64_t align_threshold = 1048576;
  // The allocation Granularity for mmap() support.
  // Typically 64KB for Windows & 4KB for other OSes. Default to 64KB.
#ifdef _WIN32
  int64_t allocation_granularity = 65536;
#else
  int64_t allocation_granularity = 4096;
#endif
};

}  // namespace onnxruntime
