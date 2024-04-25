// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FILL_LAYOUT_H_
#define UI_VIEWS_LAYOUT_FILL_LAYOUT_H_

#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/view.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
//
// FillLayout
//  A simple LayoutManager that causes the associated view's children to be
//  sized to match the bounds of its parent. The preferred size/height is
//  is calculated as the maximum values across all child views of the host.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT FillLayout : public LayoutManagerBase {
 public:
  FillLayout();

  FillLayout(const FillLayout&) = delete;
  FillLayout& operator=(const FillLayout&) = delete;

  ~FillLayout() override;

  bool minimum_size_enabled() const { return minimum_size_enabled_; }
  FillLayout& SetMinimumSizeEnabled(bool minimum_size_enabled);

  bool include_insets() const { return include_insets_; }
  FillLayout& SetIncludeInsets(bool include_insets);

  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;
  gfx::Size GetPreferredSize(const View* host) const override;
  gfx::Size GetPreferredSize(const View* host,
                             const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;

 private:
  // Returns the size bounds of the content area of the view.
  SizeBounds GetContentsSizeBounds(const View* host) const;

  // Whether to compute minimum size separately, as the maximum of all of the
  // included child views' minimum size (true), or to simply return the
  // preferred size (false).
  //
  // Off by default for backwards-compatibility with legacy uses of FillLayout.
  bool minimum_size_enabled_ = false;

  // TODO (crbug.com/327247047): Should this even be necessary?
  // Whether to include the host insets in the preferred size calculations.
  // Set to off for backwards-compatibility with View default fill layout.
  bool include_insets_ = true;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_FILL_LAYOUT_H_
