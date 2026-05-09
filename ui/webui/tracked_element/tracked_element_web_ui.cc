// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/tracked_element_web_ui.h"

#include <utility>

#include "base/check.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/native_ui_util.h"
#include "ui/views/interaction/view_subregion_anchor.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"

namespace ui {

TrackedElementVisibilityLock::TrackedElementVisibilityLock(
    base::WeakPtr<TrackedElementWebUI> element)
    : element_(std::move(element)) {
  if (element_) {
    element_->AddVisibilityLock();
  }
}

TrackedElementVisibilityLock::~TrackedElementVisibilityLock() {
  if (element_) {
    element_->RemoveVisibilityLock();
  }
}

TrackedElementVisibilityLock::TrackedElementVisibilityLock(
    TrackedElementVisibilityLock&& other) noexcept
    : element_(std::move(other.element_)) {
  other.element_.reset();
}

TrackedElementVisibilityLock& TrackedElementVisibilityLock::operator=(
    TrackedElementVisibilityLock&& other) noexcept {
  if (this != &other) {
    if (element_) {
      element_->RemoveVisibilityLock();
    }
    element_ = std::move(other.element_);
    other.element_.reset();
  }
  return *this;
}

TrackedElementWebUI::HighlightHandle::HighlightHandle(
    base::WeakPtr<TrackedElementWebUI> element)
    : element_(std::move(element)) {}

TrackedElementWebUI::HighlightHandle::~HighlightHandle() {
  if (element_) {
    element_->ReleaseHighlightHandle();
  }
}

TrackedElementWebUI::TrackedElementWebUI(TrackedElementHandler* handler,
                                         ui::ElementIdentifier identifier,
                                         ui::ElementContext context)
    : TrackedElement(identifier, context), handler_(handler) {
  DCHECK(handler);
}

TrackedElementWebUI::~TrackedElementWebUI() {
  raw_visible_ = false;
  UpdateEffectiveVisibility();
}

gfx::Rect TrackedElementWebUI::GetScreenBounds() const {
  gfx::Rect result;
  content::WebContents* const contents = handler_->web_contents();
  if (contents) {
    // Use the last known bounds, but if the bounds are empty, make them 1x1 so
    // there's something to anchor to.
    result = gfx::ToRoundedRect(last_known_bounds_);
    if (result.width() < 1) {
      result.set_width(1);
    }
    if (result.height() < 1) {
      result.set_height(1);
    }
    // To get the screen coordinates, have to offset by the coordinates of the
    // viewport.
    result.Offset(contents->GetContainerBounds().OffsetFromOrigin());
  }
  return result;
}

gfx::NativeView TrackedElementWebUI::GetNativeView() const {
  auto* const contents = handler_->web_contents();
  return (contents && contents->GetTopLevelNativeWindow())
             ? gfx::GetViewForWindow(contents->GetTopLevelNativeWindow())
             : gfx::NativeView();
}

scoped_refptr<TrackedElementWebUI::HighlightHandle>
TrackedElementWebUI::GetOrMakeHighlightHandle() {
  DCHECK(can_highlight_);

  if (highlight_handle_) {
    return highlight_handle_.get();
  }
  auto result = base::WrapRefCounted<HighlightHandle>(
      new HighlightHandle(weak_ptr_factory_.GetWeakPtr()));
  highlight_handle_ = result.get();
  handler_->SetHighlightState(identifier().GetName(), true);
  return result;
}

void TrackedElementWebUI::ReleaseHighlightHandle() {
  DCHECK(highlight_handle_);
  highlight_handle_ = nullptr;
  handler_->SetHighlightState(identifier().GetName(), false);
}

std::unique_ptr<TrackedElementVisibilityLock>
TrackedElementWebUI::LockVisible() {
  return std::make_unique<TrackedElementVisibilityLock>(
      weak_ptr_factory_.GetWeakPtr());
}

void TrackedElementWebUI::AddVisibilityLock() {
  ++visibility_lock_count_;
  UpdateEffectiveVisibility();
}

void TrackedElementWebUI::RemoveVisibilityLock() {
  DCHECK_GT(visibility_lock_count_, 0);
  --visibility_lock_count_;
  UpdateEffectiveVisibility();
}

void TrackedElementWebUI::SetRawVisible(bool visible, gfx::RectF bounds) {
  const bool bounds_changed = bounds != last_known_bounds_;
  raw_visible_ = visible;
  last_known_bounds_ = bounds;
  UpdateEffectiveVisibility(bounds_changed);
}

void TrackedElementWebUI::UpdateEffectiveVisibility(bool bounds_changed) {
  const bool visible = raw_visible_ && (handler_->is_web_contents_visible() ||
                                        visibility_lock_count_ > 0);

  if (visible == visible_) {
    if (visible && bounds_changed) {
      // This event signals that the bounds of the element have been updated.
      ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          this, kElementBoundsChangedEvent);
      ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          this, views::ViewSubregionAnchor::kAnchorBoundsChangedEvent);
    }
    return;
  }

  visible_ = visible;
  auto* const delegate = ui::ElementTracker::GetFrameworkDelegate();
  if (visible) {
    delegate->NotifyElementShown(this);
  } else {
    delegate->NotifyElementHidden(this);
  }
}

void TrackedElementWebUI::Activate() {
  DCHECK(visible_);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(this);
}

void TrackedElementWebUI::CustomEvent(ui::CustomElementEventType event_type) {
  DCHECK(visible_);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(this,
                                                                event_type);
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementWebUI)

}  // namespace ui
