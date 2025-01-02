// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/framework/allocator.h"

namespace onnxruntime {

// TODO: Do we need this class or is IAllocator::MakeUniquePtr sufficient/better
class BufferDeleter {
 public:
  BufferDeleter() = default;
  explicit BufferDeleter(AllocatorPtr alloc)
      : alloc_(std::move(alloc)) {}

  void operator()(void* p) const {
    if (alloc_)
      alloc_->Free(p);
  }

 private:
  // TODO: we may need consider the lifetime of alloc carefully
  // The alloc_ here is the allocator that used to allocate the buffer
  // And need go with the unique_ptr together. If it is using our internal
  // allocator, it is ok as our allocators are global managed. But if it
  // is provide by user, user need to be very careful about it.
  // A weak_ptr may be a choice to reduce the impact, but that require to
  // change our current allocator mgr to use shared_ptr. Will revisit it
  // later.
  AllocatorPtr alloc_{nullptr};
};

using BufferUniquePtr = std::unique_ptr<void, BufferDeleter>;
using BufferNakedPtr = void*;
}  // namespace onnxruntime
