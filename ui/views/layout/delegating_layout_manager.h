// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_DELEGATING_LAYOUT_MANAGER_H_
#define UI_VIEWS_LAYOUT_DELEGATING_LAYOUT_MANAGER_H_

#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT LayoutDelegate {
 public:
  virtual ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const = 0;

 protected:
  virtual ~LayoutDelegate() = default;
};

// The DelegatingLayoutManager is a simple, lighterweight layout manager for use
// in place of overriding the now deprecated View::Layout(). See
// LayoutManagerBase for information on calculating a proposed layout. Direct
// mutation of child views should not be done directly from within
// CalculateProposedLayout.
class VIEWS_EXPORT DelegatingLayoutManager final : public LayoutManagerBase {
 public:
  explicit DelegatingLayoutManager(LayoutDelegate* delegate);
  DelegatingLayoutManager(const DelegatingLayoutManager&) = delete;
  DelegatingLayoutManager& operator=(const DelegatingLayoutManager&) = delete;
  ~DelegatingLayoutManager() override;

 protected:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

 private:
  raw_ptr<LayoutDelegate> delegate_;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_DELEGATING_LAYOUT_MANAGER_H_
