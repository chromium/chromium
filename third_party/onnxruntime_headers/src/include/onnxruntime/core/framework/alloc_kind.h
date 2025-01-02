// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <iosfwd>

namespace onnxruntime {
// The ml-Values fall into the following categories with respect to their
// memory management:
//   - inference inputs: owned (allocated and freed) by caller, and is by
//     default read-only by the runtime.
//   - inference outputs: allocated by runtime, ownership transferred to
//     caller. TODO: Make sure this semantics is clear in InferenceSession API.
//   - weights (constant tensors): can be allocated once (statically), and
//     reused by all inference calls within an InferenceSession.
//   - tensor values: The lifetimes of these tensor-values are statically
//     determined, which is used for memory reuse/sharing optimizations. The
//     runtime allocates/frees these values at the right time (as determined
//     by the static allocation plan). Note that this is simplified since we
//     do not try to optimize for "slice" like ops, where we may be able to
//     conditionally reuse memory/data in some cases but not others.
//     Generalizing this is future work.

enum class AllocKind {
  kNotSet = -1,
  kAllocate = 0,
  kReuse = 1,
  kPreExisting = 2,
  kAllocateStatically = 3,
  kAllocateOutput = 4,
  kShare = 5,
  kAllocatedExternally = 6
};

std::ostream& operator<<(std::ostream& out, AllocKind alloc_kind);
}  // namespace onnxruntime
