// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/delegating_layout_manager.h"

namespace views {

DelegatingLayoutManager::DelegatingLayoutManager(LayoutDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

DelegatingLayoutManager::~DelegatingLayoutManager() = default;

ProposedLayout DelegatingLayoutManager::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  return delegate_->CalculateProposedLayout(size_bounds);
}

}  // namespace views
