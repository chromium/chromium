// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_VIEW_SUBREGION_ANCHOR_H_
#define UI_VIEWS_INTERACTION_VIEW_SUBREGION_ANCHOR_H_

#include <string>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_export.h"

namespace views {

// Creates a synthetic anchor element which tracks a subregion of a view.
class VIEWS_EXPORT ViewSubregionAnchor : public ui::TrackedElement {
 public:
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kAnchorBoundsChangedEvent);
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // Creates a synthetic help bubble anchor with identifier `id` that tracks the
  // position and visibility of `host`, maintaining a relative position inside
  // the view.
  //
  // The context will be read from `host`, so the view must be on a widget
  // before this is created; this object should also not outlive the view.
  ViewSubregionAnchor(ui::ElementIdentifier id, views::View& host);
  ~ViewSubregionAnchor() override;

  // Specifies the anchor region within the view.
  void MaybeUpdateAnchor(gfx::Rect local_anchor_region);

  // Switches the host view to `new_host`, optionally updating the anchor to
  // `new_anchor_region`.
  void MoveTo(View& new_host,
              std::optional<gfx::Rect> new_anchor_region = std::nullopt);

  // Gets the view associated with this element. The view must exist;
  View& view() {
    CHECK(host_) << "Host view was previously destroyed. This element should "
                    "not be visible or available through ElementTracker.";
    return *host_.view();
  }

  // Sets a "manually hidden" state in which this anchor does not register as
  // visible even when the underlying view is visible.
  void SetHidden(bool hidden);

  // ui::TrackedElement:
  gfx::Rect GetScreenBounds() const override;
  gfx::NativeView GetNativeView() const override;
  std::string ToString() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ViewSubregionAnchorTest, DeleteHost);
  FRIEND_TEST_ALL_PREFIXES(ViewSubregionAnchorTest, DeleteHostDoesNotCauseUAF);

  void OnAnchorViewShown(ui::TrackedElement* el);
  void OnAnchorViewHidden(ui::TrackedElement* el);

  void SetHost(View& view);
  void UpdateVisibility();

  bool visible_ = false;
  bool view_visible_ = false;
  bool manually_hidden_ = false;
  gfx::Rect last_anchor_region_;
  ViewTracker host_;
  base::CallbackListSubscription anchor_view_shown_subscription_;
  base::CallbackListSubscription anchor_view_hidden_subscription_;
};

}  // namespace views

#endif  // UI_VIEWS_INTERACTION_VIEW_SUBREGION_ANCHOR_H_
