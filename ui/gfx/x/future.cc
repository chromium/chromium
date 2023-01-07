// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/future.h"

namespace x11 {

FutureBase::FutureBase() = default;

FutureBase::FutureBase(std::unique_ptr<Connection::FutureImpl> impl)
    : impl_(std::move(impl)) {}

FutureBase::FutureBase(FutureBase&&) = default;

FutureBase& FutureBase::operator=(FutureBase&&) = default;

FutureBase::~FutureBase() = default;

void FutureBase::Wait() {
  if (impl_)
    impl_->Wait();
}

void FutureBase::DispatchNow() {
  if (impl_)
    impl_->DispatchNow();
}

bool FutureBase::AfterEvent(const Event& event) const {
  return impl_ ? impl_->AfterEvent(event) : false;
}

}  // namespace x11