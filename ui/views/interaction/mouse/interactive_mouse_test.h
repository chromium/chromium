// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_MOUSE_INTERACTIVE_MOUSE_TEST_H_
#define UI_VIEWS_INTERACTION_MOUSE_INTERACTIVE_MOUSE_TEST_H_

#include <utility>

#include "ui/base/interaction/interactive_test.h"
#include "ui/views/interaction/mouse/interaction_test_util_mouse.h"
#include "ui/views/interaction/mouse/interactive_mouse_test_internal.h"

namespace views::test {

// Provides interactive test functionality for Views.
//
// Interactive tests use InteractionSequence, ElementTracker, and
// InteractionTestUtil to provide a common library of concise test methods. This
// convenience API is nicknamed "Kombucha" (see
// //chrome/test/interaction/README.md for more information).
//
// This class is not a test fixture; it is a mixin that can be added to existing
// test classes using `InteractiveMouseTestMixin<T>`.
//
// To use Kombucha for in-process browser tests, instead see:
// //chrome/test/interaction/interactive_browser_test.h
class InteractiveMouseTestApi : virtual public ui::test::InteractiveTestApi {
 public:
  InteractiveMouseTestApi();
  ~InteractiveMouseTestApi() override;

  // Returns an object that can be used to inject mouse inputs. Generally,
  // prefer to use methods like MoveMouseTo, MouseClick, and DragMouseTo.
  InteractionTestUtilMouse& mouse_util() { return test_impl_->mouse_util(); }

  // Indicates that the center point of the target element should be used for a
  // mouse move.
  struct CenterPoint {};

  // Function that returns a destination for a move or drag.
  using AbsolutePositionCallback = base::OnceCallback<gfx::Point()>;

  // Specifies an absolute position for a mouse move or drag that does not need
  // a reference element.
  using AbsolutePositionSpecifier = std::variant<
      // Use this specific position. This value is stored when the sequence is
      // created; use gfx::Point* if you want to capture a point during sequence
      // execution.
      gfx::Point,
      // As above, but the position is read from the reference on execution
      // instead of copied when the test sequence is constructed. Use this when
      // you want to calculate and cache a point during test execution for later
      // use. The pointer must remain valid through the end of the test.
      std::reference_wrapper<gfx::Point>,
      // Use the return value of the supplied callback
      AbsolutePositionCallback>;

  // Specifies how the `reference_element` should be used (or not) to generate a
  // target point for a mouse move.
  using RelativePositionCallback =
      base::OnceCallback<gfx::Point(ui::TrackedElement* reference_element)>;

  // Specifies how the target position of a mouse operation (in screen
  // coordinates) will be determined.
  using RelativePositionSpecifier = std::variant<
      // Default to the centerpoint of the reference element, which should be a
      // views::View.
      CenterPoint,
      // Use the return value of the supplied callback.
      RelativePositionCallback>;

  // Move the mouse to the specified `position` in screen coordinates. The
  // `reference` element will be used based on how `position` is specified.
  //
  // This verb is only available in interactive test suites; see
  // `RequireInteractiveTest()`.
  [[nodiscard]] StepBuilder MoveMouseTo(AbsolutePositionSpecifier position);
  [[nodiscard]] StepBuilder MoveMouseTo(
      ElementSpecifier reference,
      RelativePositionSpecifier position = CenterPoint());

  // Clicks mouse button `button` at the current cursor position.
  //
  // This verb is only available in interactive test suites; see
  // `RequireInteractiveTest()`.
  //
  // The optional `modifier_keys` parameter can be set to any combination of
  // `ui_controls::AcceleratorState`.
  [[nodiscard]] StepBuilder ClickMouse(
      ui_controls::MouseButton button = ui_controls::LEFT,
      bool release = true,
      int modifier_keys = ui_controls::kNoAccelerator);

  // Depresses the left mouse button at the current cursor position and drags to
  // the target `position`. The `reference` element will be used based on how
  // `position` is specified.
  //
  // This verb is only available in interactive test suites; see
  // `RequireInteractiveTest()`.
  [[nodiscard]] StepBuilder DragMouseTo(AbsolutePositionSpecifier position,
                                        bool release = true);
  [[nodiscard]] StepBuilder DragMouseTo(
      ElementSpecifier reference,
      RelativePositionSpecifier position = CenterPoint(),
      bool release = true);

  // Releases the specified mouse button. Use when you previously called
  // ClickMouse() or DragMouseTo() with `release` = false.
  //
  // This verb is only available in interactive test suites; see
  // `RequireInteractiveTest()`.
  //
  // The optional `modifier_keys` parameter can be set to any combination of
  // `ui_controls::AcceleratorState`.
  [[nodiscard]] StepBuilder ReleaseMouse(
      ui_controls::MouseButton button = ui_controls::LEFT,
      int modifier_keys = ui_controls::kNoAccelerator);

 private:
  // Converts a *PositionSpecifier to an appropriate *PositionCallback.
  static RelativePositionCallback GetPositionCallback(
      AbsolutePositionSpecifier spec);
  static RelativePositionCallback GetPositionCallback(
      RelativePositionSpecifier spec);

  // Creates the follow-up step for a mouse action.
  StepBuilder CreateMouseFollowUpStep(std::string_view description);

  const raw_ptr<internal::InteractiveMouseTestPrivate> test_impl_;
};

// Template that adds InteractiveMouseTestApi to any test fixture.
//
// You must call SetContextWidget() before using RunTestSequence() or any of the
// mouse actions.
//
// See //chrome/test/interaction/README.md for usage.
template <typename T>
class InteractiveMouseTestMixin : public T, public InteractiveMouseTestApi {
 public:
  template <typename... Args>
  explicit InteractiveMouseTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~InteractiveMouseTestMixin() override = default;

 protected:
  void SetUp() override {
    T::SetUp();
    private_test_impl().DoTestSetUp();
  }

  void TearDown() override {
    private_test_impl().DoTestTearDown();
    T::TearDown();
  }
};

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_MOUSE_INTERACTIVE_MOUSE_TEST_H_
