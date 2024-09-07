// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/client_view.h"

#include <memory>

#include "base/check.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
// ClientView, public:

ClientView::ClientView(Widget* widget, View* contents_view)
    : contents_view_(contents_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  GetViewAccessibility().SetRole(ax::mojom::Role::kClient);
}

CloseRequestResult ClientView::OnWindowCloseRequested() {
  return CloseRequestResult::kCanClose;
}

void ClientView::WidgetClosing() {}

int ClientView::NonClientHitTest(const gfx::Point& point) {
  return bounds().Contains(point) ? HTCLIENT : HTNOWHERE;
}

void ClientView::UpdateWindowRoundedCorners(int corner_radius) {}

///////////////////////////////////////////////////////////////////////////////
// ClientView, View overrides:

gfx::Size ClientView::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  if (!contents_view_) {
    return gfx::Size();
  }

  return contents_view_->GetPreferredSize(available_size);
}

gfx::Size ClientView::GetMaximumSize() const {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  return contents_view_ ? contents_view_->GetMaximumSize() : gfx::Size();
}

gfx::Size ClientView::GetMinimumSize() const {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  return contents_view_ ? contents_view_->GetMinimumSize() : gfx::Size();
}

void ClientView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Overridden to do nothing. The NonClientView manually calls Layout on the
  // ClientView when it is itself laid out, see comment in
  // NonClientView::Layout.
}

void ClientView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    DCHECK(GetWidget());
    DCHECK(contents_view_);  // |contents_view_| must be valid now!
    // Insert |contents_view_| at index 0 so it is first in the focus chain.
    // (the OK/Cancel buttons are inserted before contents_view_)
    // TODO(weili): This seems fragile and can be refactored.
    // Tracked at https://crbug.com/1012466.
    AddChildViewAt(contents_view_.get(), 0);
  }
}

BEGIN_METADATA(ClientView)
END_METADATA

}  // namespace views
