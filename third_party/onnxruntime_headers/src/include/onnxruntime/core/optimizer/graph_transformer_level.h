// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

namespace onnxruntime {

// Graph transformer level
// refer to docs/ONNX_Runtime_Graph_Optimizations.md for details
enum class TransformerLevel : int {
  Default = 0,  // required transformers only
  Level1,       // basic optimizations
  Level2,       // extended optimizations
  Level3,       // layout optimizations
  // The max level should always be same as the last level.
  MaxLevel = Level3
};

}  // namespace onnxruntime
