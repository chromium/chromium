// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interactive_views_test_internal.h"

#include <memory>
#include <utility>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/native_window_tracker.h"
#include "ui/views/widget/widget.h"

namespace views::test::internal {

// Caches the last-known native window associated with a context.
// Useful for executing ClickMouse() and ReleaseMouse() commands, as no target
// element is provided for those commands. A NativeWindowTracker is used to
// prevent using a cached value after the native window has been destroyed.
class InteractiveViewsTestPrivate::WindowHintCacheEntry {
 public:
  WindowHintCacheEntry() = default;
  ~WindowHintCacheEntry() = default;
  WindowHintCacheEntry(WindowHintCacheEntry&& other) = default;
  WindowHintCacheEntry& operator=(WindowHintCacheEntry&& other) = default;

  bool IsValid() const {
    return window_ && tracker_ && !tracker_->WasNativeWindowDestroyed();
  }

  gfx::NativeWindow GetWindow() const {
    return IsValid() ? window_ : gfx::NativeWindow();
  }

  void SetWindow(gfx::NativeWindow window) {
    if (window_ == window)
      return;
    window_ = window;
    tracker_ = window ? views::NativeWindowTracker::Create(window) : nullptr;
  }

 private:
  gfx::NativeWindow window_ = gfx::NativeWindow();
  std::unique_ptr<NativeWindowTracker> tracker_;
};

InteractiveViewsTestPrivate::InteractiveViewsTestPrivate(
    std::unique_ptr<ui::test::InteractionTestUtil> test_util)
    : InteractiveTestPrivate(std::move(test_util)) {}
InteractiveViewsTestPrivate::~InteractiveViewsTestPrivate() = default;

void InteractiveViewsTestPrivate::DoTestTearDown() {
  mouse_util_.reset();
  InteractiveTestPrivate::DoTestTearDown();
}

void InteractiveViewsTestPrivate::OnSequenceComplete() {
  mouse_util_->CancelAllGestures();
  InteractiveTestPrivate::OnSequenceComplete();
}

void InteractiveViewsTestPrivate::OnSequenceAborted(
    int active_step,
    ui::TrackedElement* last_element,
    ui::ElementIdentifier last_id,
    ui::InteractionSequence::StepType last_step_type,
    ui::InteractionSequence::AbortedReason aborted_reason,
    std::string description) {
  mouse_util_->CancelAllGestures();
  InteractiveTestPrivate::OnSequenceAborted(active_step, last_element, last_id,
                                            last_step_type, aborted_reason,
                                            description);
}

gfx::NativeWindow InteractiveViewsTestPrivate::GetWindowHintFor(
    ui::TrackedElement* el) {
  // See if the native window can be extracted directly from the element.
  gfx::NativeWindow window = GetNativeWindowFromElement(el);

  // If not, see if the window can be extracted from the context (perhaps via
  // the cache).
  if (!window)
    window = GetNativeWindowFromContext(el->context());

  // If a window was found, then a cache entry may need to be inserted/updated.
  if (window) {
    // This is just a find if the entry already exists.
    auto result =
        window_hint_cache_.try_emplace(el->context(), WindowHintCacheEntry());
    // This is a no-op if this is already the cached window.
    result.first->second.SetWindow(window);
  }

  return window;
}

gfx::NativeWindow InteractiveViewsTestPrivate::GetNativeWindowFromElement(
    ui::TrackedElement* el) const {
  gfx::NativeWindow window = gfx::NativeWindow();
  if (el->IsA<TrackedElementViews>()) {
    // Most widgets have an associated native window.
    Widget* const widget = el->AsA<TrackedElementViews>()->view()->GetWidget();
    window = widget->GetNativeWindow();
    // Most of those that don't are sub-widgets that are hard-parented to
    // another widget.
    if (!window && widget->parent())
      window = widget->parent()->GetNativeWindow();
    // At worst case, fall back to the primary window.
    if (!window)
      window = widget->GetPrimaryWindowWidget()->GetNativeWindow();
  }
  return window;
}

gfx::NativeWindow InteractiveViewsTestPrivate::GetNativeWindowFromContext(
    ui::ElementContext context) const {
  // Used the cached value, if one exists.
  const auto it = window_hint_cache_.find(context);
  return it != window_hint_cache_.end() ? it->second.GetWindow()
                                        : gfx::NativeWindow();
}

}  // namespace views::test::internal
