// Copyright 2022 The Chromium Authors. All rights reserved.
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

}  // namespace x11