// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interactive_views_test_internal.h"

#include <memory>
#include <utility>

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/native_window_tracker.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/test/aura_test_helper.h"
#endif

namespace views::test::internal {

namespace {

// Basic observer for low-level activation changes. Relays when a widget
// receives focus.
class NativeViewWidgetFocusSupplier : public WidgetFocusSupplier,
                                      public WidgetFocusChangeListener {
 public:
  NativeViewWidgetFocusSupplier() {
    observation_.Observe(WidgetFocusManager::GetInstance());
  }
  ~NativeViewWidgetFocusSupplier() override = default;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  void OnNativeFocusChanged(gfx::NativeView focused_now) override {
    // TODO(dfried): There's an order-of-operations issue on some platforms
    // where focus transfers between two native views, and the blur for the old
    // view is received after the focus for the new view. This results in
    // `focused_now` being null rather than the currently-focused view.
    //
    // While it's slightly less correct, ignore blur events until this can be
    // fixed. In general, one would not expect windows not from the application
    // under test to become focused, so this will be a valid choice most of the
    // time.
    if (focused_now) {
      OnWidgetFocusChanged(focused_now);
    }
  }

 protected:
  Widget::Widgets GetAllWidgets() const override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On Ash, WidgetTest::GetAllWidgets() requires special test utils to be set
    // up that are incompatible with browser tests. If a test helper has been
    // set up, then use it, otherwise assume that the browser version will
    // handle fetching the widgets.
    Widget::Widgets result;
    if (aura::test::AuraTestHelper* const aura_test_helper =
            aura::test::AuraTestHelper::GetInstance()) {
      Widget::GetAllChildWidgets(aura_test_helper->GetContext(), &result);
    }
    return result;
#else
    return WidgetTest::GetAllWidgets();
#endif
  }

 private:
  base::ScopedObservation<WidgetFocusManager, WidgetFocusChangeListener>
      observation_{this};
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(NativeViewWidgetFocusSupplier)

}  // namespace

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

void InteractiveViewsTestPrivate::OnSequenceComplete() {
  if (mouse_util_) {
    mouse_util_->CancelAllGestures();
  }
  InteractiveTestPrivate::OnSequenceComplete();
}

void InteractiveViewsTestPrivate::OnSequenceAborted(
    const ui::InteractionSequence::AbortedData& data) {
  if (mouse_util_) {
    mouse_util_->CancelAllGestures();
  }
  InteractiveTestPrivate::OnSequenceAborted(data);
}

void InteractiveViewsTestPrivate::DoTestSetUp() {
  InteractiveTestPrivate::DoTestSetUp();
  // Frame should exist from set up to tear down, to prevent framework/system
  // listeners from receiving events outside of the test.
  widget_focus_supplier_frame_ = std::make_unique<WidgetFocusSupplierFrame>();
  widget_focus_suppliers().MaybeRegister<NativeViewWidgetFocusSupplier>();
}

void InteractiveViewsTestPrivate::DoTestTearDown() {
  // Avoid doing any widget focus tracking after the test completes.
  widget_focus_supplier_frame_.reset();
  InteractiveTestPrivate::DoTestTearDown();
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
