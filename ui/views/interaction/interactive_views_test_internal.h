// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_INTERNAL_H_
#define UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_INTERNAL_H_

#include <concepts>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/interaction/interaction_test_util_mouse.h"
#include "ui/views/interaction/widget_focus_observer.h"

namespace views {
class NativeWindowTracker;
class View;
}

namespace views::test {

class InteractiveViewsTestApi;

namespace internal {

// Provides functionality required by InteractiveViewsTestApi but which needs to
// be hidden from tests inheriting from the API class.
class InteractiveViewsTestPrivate
    : public ui::test::internal::InteractiveTestPrivate {
 public:
  explicit InteractiveViewsTestPrivate(
      std::unique_ptr<ui::test::InteractionTestUtil> test_util);
  ~InteractiveViewsTestPrivate() override;

  // base::test::internal::InteractiveTestPrivate:
  void OnSequenceComplete() override;
  void OnSequenceAborted(
      const ui::InteractionSequence::AbortedData& data) override;
  void DoTestSetUp() override;
  void DoTestTearDown() override;

  InteractionTestUtilMouse& mouse_util() { return *mouse_util_; }

  gfx::NativeWindow GetWindowHintFor(ui::TrackedElement* el);

 protected:
  // Retrieves the native window from an element. Used by GetWindowHintFor().
  virtual gfx::NativeWindow GetNativeWindowFromElement(
      ui::TrackedElement* el) const;

  // Retrieves the native window from a context. Used by GetWindowHintFor().
  virtual gfx::NativeWindow GetNativeWindowFromContext(
      ui::ElementContext context) const;

  // Use this to register widget focus suppliers.
  WidgetFocusSupplierFrame::SupplierList& widget_focus_suppliers() {
    return widget_focus_supplier_frame_->supplier_list();
  }

 private:
  friend class views::test::InteractiveViewsTestApi;

  class WindowHintCacheEntry;

  // Provides mouse input simulation.
  std::unique_ptr<InteractionTestUtilMouse> mouse_util_;

  // Tracks failures when a mouse operation fails.
  std::string mouse_error_message_;

  // Safely tracks the most recent native window targeted in each context.
  // For actions like ClickMouse() or ReleaseMouse(), a pivot element is used
  // so only the context is known; a NativeWindowTracker must be used to verify
  // that the cached information is still valid.
  std::map<ui::ElementContext, WindowHintCacheEntry> window_hint_cache_;

  std::unique_ptr<WidgetFocusSupplierFrame> widget_focus_supplier_frame_;
};

template <typename T>
concept IsView = std::derived_from<T, View>;

template <size_t N,
          typename F,
          typename V = std::remove_cv_t<
              std::remove_pointer_t<ui::test::internal::NthArgumentOf<N, F>>>>
  requires IsView<V>
using ViewArgType = V;

}  // namespace internal

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_INTERNAL_H_
