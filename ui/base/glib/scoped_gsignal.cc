// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/glib/scoped_gsignal.h"

ScopedGSignal::ScopedGSignal() = default;

ScopedGSignal::ScopedGSignal(ScopedGSignal&& other) noexcept = default;

ScopedGSignal& ScopedGSignal::operator=(ScopedGSignal&& other) noexcept =
    default;

ScopedGSignal::~ScopedGSignal() = default;

bool ScopedGSignal::Connected() const {
  return impl_ && impl_->Connected();
}

void ScopedGSignal::Reset() {
  impl_.reset();
}

ScopedGSignal::SignalBase::SignalBase() = default;

ScopedGSignal::SignalBase::~SignalBase() = default;
