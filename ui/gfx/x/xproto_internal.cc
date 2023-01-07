// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_internal.h"

#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

namespace x11 {

MallocedRefCountedMemory::MallocedRefCountedMemory(void* data)
    : data_(reinterpret_cast<uint8_t*>(data)) {}

const uint8_t* MallocedRefCountedMemory::front() const {
  return data_.get();
}

size_t MallocedRefCountedMemory::size() const {
  // There's no easy way to tell how large malloc'ed data is.
  NOTREACHED();
  return 0;
}

MallocedRefCountedMemory::~MallocedRefCountedMemory() = default;

OffsetRefCountedMemory::OffsetRefCountedMemory(
    scoped_refptr<base::RefCountedMemory> memory,
    size_t offset,
    size_t size)
    : memory_(memory), offset_(offset), size_(size) {}

const uint8_t* OffsetRefCountedMemory::front() const {
  return memory_->front() + offset_;
}

size_t OffsetRefCountedMemory::size() const {
  return size_;
}

OffsetRefCountedMemory::~OffsetRefCountedMemory() = default;

UnretainedRefCountedMemory::UnretainedRefCountedMemory(const void* data)
    : data_(reinterpret_cast<const uint8_t*>(data)) {}

const uint8_t* UnretainedRefCountedMemory::front() const {
  return data_;
}

size_t UnretainedRefCountedMemory::size() const {
  // There's no easy way to tell how large malloc'ed data is.
  NOTREACHED();
  return 0;
}

UnretainedRefCountedMemory::~UnretainedRefCountedMemory() = default;

}  // namespace x11
