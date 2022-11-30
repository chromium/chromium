// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/ref_counted.h"

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

RefCounted::RefCounted() = default;

RefCounted::~RefCounted() = default;

void RefCounted::AcquireRef() {
  ref_count_.fetch_add(1, std::memory_order_relaxed);
}

void RefCounted::ReleaseRef() {
  // SUBTLE: Technically the load does not need to be an acquire unless we're
  // releasing the last reference and need to delete `this`, but it's not clear
  // whether std::memory_order_acq_rel here will produce more or less efficient
  // code compared to a plain std::memory_order_release followed by an acquire
  // fence in the conditional block below.
  int last_count = ref_count_.fetch_sub(1, std::memory_order_acq_rel);
  ABSL_ASSERT(last_count > 0);
  if (last_count == 1) {
    delete this;
  }
}

GenericRef::GenericRef(decltype(RefCounted::kAdoptExistingRef), RefCounted* ptr)
    : ptr_(ptr) {}

GenericRef::GenericRef(RefCounted* ptr) : ptr_(ptr) {
  if (ptr_) {
    ptr_->AcquireRef();
  }
}

GenericRef::GenericRef(GenericRef&& other)
    : ptr_(std::exchange(other.ptr_, nullptr)) {}

GenericRef& GenericRef::operator=(GenericRef&& other) {
  reset();
  ptr_ = std::exchange(other.ptr_, nullptr);
  return *this;
}

GenericRef::GenericRef(const GenericRef& other) : ptr_(other.ptr_) {
  if (ptr_) {
    ptr_->AcquireRef();
  }
}

GenericRef& GenericRef::operator=(const GenericRef& other) {
  reset();
  ptr_ = other.ptr_;
  if (ptr_) {
    ptr_->AcquireRef();
  }
  return *this;
}

GenericRef::~GenericRef() {
  reset();
}

void GenericRef::reset() {
  if (ptr_) {
    std::exchange(ptr_, nullptr)->ReleaseRef();
  }
}

void* GenericRef::ReleaseImpl() {
  return std::exchange(ptr_, nullptr);
}

}  // namespace ipcz
