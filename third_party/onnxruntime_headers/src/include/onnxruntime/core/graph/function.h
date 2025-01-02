// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/common/common.h"
#include "core/graph/indexed_sub_graph.h"

namespace onnxruntime {
class Graph;
class Node;
}  // namespace onnxruntime

namespace onnxruntime {

/**
@class Function
Class representing a Function.
*/
class Function {
 public:
  virtual ~Function() = default;

  /** Gets the Graph instance for the Function body subgraph. */
  virtual const onnxruntime::Graph& Body() const = 0;

  /** Gets the Mutable Graph instance for the Function body subgraph. */
  virtual onnxruntime::Graph& MutableBody() = 0;
};

}  // namespace onnxruntime
