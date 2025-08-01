// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANCHOR_VIEW_ANCHOR_H_
#define UI_VIEWS_ANCHOR_VIEW_ANCHOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/anchor/anchor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT ViewAnchorImpl : public ui::AnchorImpl {
 public:
  explicit ViewAnchorImpl(View* anchor_view);
  ~ViewAnchorImpl() override;
  std::unique_ptr<ui::AnchorImpl> Clone() const override;
  bool IsEmpty() const override;
  gfx::Rect GetScreenBounds() const override;
  Widget* GetWidget() override;
  bool IsView() const override;
  View* GetView() override;

 private:
  View* view() { return view_tracker_.view(); }
  const View* view() const { return view_tracker_.view(); }

  ViewTracker view_tracker_;
};

}  // namespace views

#endif  // UI_VIEWS_ANCHOR_VIEW_ANCHOR_H_
