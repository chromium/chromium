// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/view_subregion_anchor.h"

#include <algorithm>

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(ViewSubregionAnchor,
                                       kAnchorBoundsChangedEvent);

DEFINE_FRAMEWORK_SPECIFIC_METADATA(ViewSubregionAnchor)

ViewSubregionAnchor::ViewSubregionAnchor(ui::ElementIdentifier id,
                                         views::View& host)
    : TrackedElement(id, views::ElementTrackerViews::GetContextForView(&host)) {
  SetHost(host);
}

ViewSubregionAnchor::~ViewSubregionAnchor() {
  manually_hidden_ = true;
  UpdateVisibility();
}

void ViewSubregionAnchor::MaybeUpdateAnchor(gfx::Rect local_anchor_region) {
  if (last_anchor_region_ == local_anchor_region) {
    return;
  }

  last_anchor_region_ = local_anchor_region;
  if (visible_) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        this, kAnchorBoundsChangedEvent);
  }
}

void ViewSubregionAnchor::MoveTo(View& new_host,
                                 std::optional<gfx::Rect> new_anchor_region) {
  if (host_.view() == &new_host) {
    if (new_anchor_region) {
      MaybeUpdateAnchor(*new_anchor_region);
    }
    return;
  }

  SetHost(new_host);

  if (new_anchor_region) {
    last_anchor_region_ = *new_anchor_region;
  }
  if (visible_) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        this, kAnchorBoundsChangedEvent);
  }
}

gfx::Rect ViewSubregionAnchor::GetScreenBounds() const {
  if (!host_) {
    return gfx::Rect();
  }
  gfx::Rect screen_bounds = last_anchor_region_;
  views::View::ConvertRectToScreen(host_.view(), &screen_bounds);
  return screen_bounds;
}

gfx::NativeView ViewSubregionAnchor::GetNativeView() const {
  if (!host_ || !host_.view()->GetWidget()) {
    return gfx::NativeView();
  }
  return host_.view()->GetWidget()->GetNativeView();
}

std::string ViewSubregionAnchor::ToString() const {
  auto result = TrackedElement::ToString();
  result.append(" tracking view ");
  result.append(host_ ? host_.view()->GetClassName() : "[none]");
  return result;
}

void ViewSubregionAnchor::SetHidden(bool hidden) {
  if (manually_hidden_ == hidden) {
    return;
  }

  manually_hidden_ = hidden;
  UpdateVisibility();
}

void ViewSubregionAnchor::OnAnchorViewShown(ui::TrackedElement* el) {
  if (auto* const view_el = el->AsA<views::TrackedElementViews>()) {
    if (view_el->view() == host_.view()) {
      view_visible_ = true;
      UpdateVisibility();
    }
  }
}

void ViewSubregionAnchor::OnAnchorViewHidden(ui::TrackedElement* el) {
  if (auto* const view_el = el->AsA<views::TrackedElementViews>()) {
    if (view_el->view() == host_.view()) {
      view_visible_ = false;
      UpdateVisibility();
    }
  }
}

void ViewSubregionAnchor::SetHost(View& view) {
  host_.SetView(&view);
  CHECK(host_);
  CHECK_EQ(context(), views::ElementTrackerViews::GetContextForView(&view))
      << "Moving an anchor to a different context is not supported.";
  const ui::ElementIdentifier view_id =
      view.GetProperty(views::kElementIdentifierKey);
  CHECK(view_id);
  anchor_view_shown_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          view_id, context(),
          base::BindRepeating(&ViewSubregionAnchor::OnAnchorViewShown,
                              base::Unretained(this)));
  anchor_view_hidden_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          view_id, context(),
          base::BindRepeating(&ViewSubregionAnchor::OnAnchorViewHidden,
                              base::Unretained(this)));
  auto candidates =
      ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
          view_id, context());
  view_visible_ = std::ranges::any_of(
      candidates, [to_find = &view](ui::TrackedElement* found) {
        auto* const view_el = found->AsA<views::TrackedElementViews>();
        return view_el && view_el->view() == to_find;
      });
  UpdateVisibility();
}

void ViewSubregionAnchor::UpdateVisibility() {
  const bool is_visible = view_visible_ && !manually_hidden_;
  if (is_visible == visible_) {
    return;
  }

  visible_ = is_visible;
  if (visible_) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(this);
  } else {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(this);
  }
}

}  // namespace views
