// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/ref_counted_fd.h"

#include "base/check_op.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"

namespace x11 {

RefCountedFD::RefCountedFD() = default;

RefCountedFD::RefCountedFD(int fd) : impl_(base::MakeRefCounted<Impl>(fd)) {}

RefCountedFD::RefCountedFD(base::ScopedFD fd)
    : impl_(base::MakeRefCounted<Impl>(std::move(fd))) {}

RefCountedFD::RefCountedFD(const RefCountedFD& other) = default;
RefCountedFD::RefCountedFD(RefCountedFD&&) = default;
RefCountedFD& RefCountedFD::operator=(const RefCountedFD& other) = default;
RefCountedFD& RefCountedFD::operator=(RefCountedFD&&) = default;
RefCountedFD::~RefCountedFD() = default;

int RefCountedFD::get() const {
  return impl_ ? impl_->fd().get() : -1;
}

RefCountedFD::Impl::Impl(int fd) : fd_(fd) {}

RefCountedFD::Impl::Impl(base::ScopedFD fd) : fd_(std::move(fd)) {}

RefCountedFD::Impl::~Impl() = default;

}  // namespace x11
