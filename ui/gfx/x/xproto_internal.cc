// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/x/xproto_internal.h"

#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

namespace x11 {

MallocedRefCountedMemory::MallocedRefCountedMemory(void* data)
    : data_(static_cast<uint8_t*>(data)) {}

MallocedRefCountedMemory::~MallocedRefCountedMemory() = default;

void* MallocedRefCountedMemory::data() {
  return data_.get();
}

const void* MallocedRefCountedMemory::data() const {
  return data_.get();
}

OffsetRefCountedMemory::OffsetRefCountedMemory(
    scoped_refptr<UnsizedRefCountedMemory> memory,
    size_t offset,
    size_t size)
    : memory_(memory), offset_(offset) {}

OffsetRefCountedMemory::~OffsetRefCountedMemory() = default;

void* OffsetRefCountedMemory::data() {
  return memory_->bytes() + offset_;
}

const void* OffsetRefCountedMemory::data() const {
  return memory_->bytes() + offset_;
}

UnretainedRefCountedMemory::UnretainedRefCountedMemory(void* data)
    : data_(data) {}

UnretainedRefCountedMemory::~UnretainedRefCountedMemory() = default;

void* UnretainedRefCountedMemory::data() {
  return data_;
}

const void* UnretainedRefCountedMemory::data() const {
  return data_;
}

}  // namespace x11
