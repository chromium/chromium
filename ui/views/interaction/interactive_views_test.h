// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_H_
#define UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_H_

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/interactive_test_definitions.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_mouse.h"
#include "ui/views/interaction/interactive_views_test_internal.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace views::test {

// Provides interactive test functionality for Views.
//
// Interactive tests use InteractionSequence, ElementTracker, and
// InteractionTestUtil to provide a common library of concise test methods. This
// convenience API is nicknamed "Kombucha" (see
// //chrome/test/interaction/README.md for more information).
//
// This class is not a test fixture; it is a mixin that can be added to existing
// test classes using `InteractiveViewsTestMixin<T>`.
//
// To use Kombucha for in-process browser tests, instead see:
// //chrome/test/interaction/interactive_browser_test.h
class InteractiveViewsTestApi : virtual public ui::test::InteractiveTestApi {
 public:
  InteractiveViewsTestApi();
  ~InteractiveViewsTestApi() override;

  // Returns an object that can be used to inject mouse inputs. Generally,
  // prefer to use methods like MoveMouseTo, MouseClick, and DragMouseTo.
  InteractionTestUtilMouse& mouse_util() { return test_impl_->mouse_util(); }

  // Shorthand to convert a tracked element into a View. The element should be
  // a views::TrackedElementViews and of type `T`.
  template <typename T = View>
  static T* AsView(ui::TrackedElement* el);
  template <typename T = View>
  static const T* AsView(const ui::TrackedElement* el);

  // Naming views:
  //
  // The following methods name a view (to be referred to later in the test
  // sequence by name) based on some kind of rule or relationship. The View need
  // not have an ElementIdentifier assigned ahead of time, so this is useful for
  // finding random or dynamically-created views.
  //
  // For example:
  //
  //   RunTestSequence(
  //     ...
  //     NameView(kThirdTabName,
  //              base::BindLambdaForTesting([&](){
  //                return browser_view->tabstrip()->tab_at(3);
  //              }))
  //     WithElement(kThirdTabName, ...)
  //     ...
  //   );
  //
  // How the view is named will depend on which version of the method you use;
  // the

  // Determines if a view matches some predicate.
  using ViewMatcher = base::RepeatingCallback<bool(const View*)>;

  // Specifies a View not relative to any particular other View.
  using AbsoluteViewSpecifier = std::variant<
      // Specify a view that is known at the time the sequence is created. The
      // View must persist until the step executes.
      View*,
      // Specify a view pointer that will be valid by the time the step
      // executes. Use `std::ref()` to wrap the pointer that will receive the
      // value.
      std::reference_wrapper<View*>,
      // Find and return a view based on an arbitrary rule.
      base::OnceCallback<View*()>>;

  // Specifies a view relative to its parent.
  using ChildViewSpecifier = std::variant<
      // The index of the child in the parent view. An out of bounds index will
      // generate an error.
      size_t,
      // Specifies a filter that is applied to the children; the first child
      // view to satisfy the filter (i.e. return true) is named.
      ViewMatcher>;

  // Methods that name views.

  // Names a view relative to another view `relative_to` based on an arbitrary
  // rule. The resulting view does not need to be a descendant (or even an
  // ancestor) of `relative_to`.
  //
  // Your `find_callback` should take a pointer to a View or a derived type and
  // return a pointer to a View or derived type.
  template <typename C,
            typename V = internal::ViewArgType<0, C>,
            typename R = std::remove_cv_t<
                std::remove_pointer_t<ui::test::internal::ReturnTypeOf<C>>>>
    requires ui::test::internal::HasSignature<C, R*(V*)>
  [[nodiscard]] static StepBuilder NameViewRelative(
      ElementSpecifier relative_to,
      std::string_view name,
      C&& find_callback);

  [[nodiscard]] static StepBuilder NameView(std::string_view name,
                                            AbsoluteViewSpecifier spec);

  [[nodiscard]] static StepBuilder NameChildView(ElementSpecifier parent,
                                                 std::string_view name,
                                                 ChildViewSpecifier spec);

  [[nodiscard]] static StepBuilder NameDescendantView(ElementSpecifier ancestor,
                                                      std::string_view name,
                                                      ViewMatcher matcher);

  // Names the `index` (0-indexed) child view of `parent` that is of type `V`.
  template <typename V>
    requires internal::IsView<V>
  [[nodiscard]] static StepBuilder NameChildViewByType(ElementSpecifier parent,
                                                       std::string_view name,
                                                       size_t index = 0);

  // Names the `index` (0-indexed) descendant view of `parent` in depth-first
  // traversal order that is of type `V`.
  template <typename V>
    requires internal::IsView<V>
  [[nodiscard]] static StepBuilder NameDescendantViewByType(
      ElementSpecifier ancestor,
      std::string_view name,
      size_t index = 0);

  // As WithElement(), but `view` should resolve to a TrackedElementViews
  // wrapping a view of type `V`.
  template <typename F, typename V = internal::ViewArgType<0, F>>
    requires ui::test::internal::HasSignature<F, void(V*)>
  [[nodiscard]] static StepBuilder WithView(ElementSpecifier view,
                                            F&& function);

  // As CheckElement(), but `view` should resolve to a TrackedElementViews
  // wrapping a view of type `V`.
  template <typename F, typename V = internal::ViewArgType<0, F>>
  // NOLINTNEXTLINE(readability/casting)
    requires ui::test::internal::HasSignature<F, bool(V*)>
  [[nodiscard]] static StepBuilder CheckView(ElementSpecifier view, F&& check);

  // As CheckView(), but checks that the result of calling `function` on `view`
  // matches `matcher`. If not, the mismatch is printed and the test fails.
  //
  // `matcher` can be any type `M` that resolves or converts to type
  // `Matcher<R>`.
  template <typename F,
            typename M,
            typename R = ui::test::internal::ReturnTypeOf<F>,
            typename V = internal::ViewArgType<0, F>>
    requires ui::test::internal::HasSignature<F, R(V*)>
  [[nodiscard]] static StepBuilder CheckView(ElementSpecifier view,
                                             F&& function,
                                             M&& matcher);

  // As CheckView() but checks that `matcher` matches the value returned by
  // calling `property` on `view`. On failure, logs the matcher error and fails
  // the test.
  //
  // `V` is the View class, `R` is the type of the property being checked, and
  // `M` is the type of the matcher, or the constant that will be converted to
  // an equality matcher (e.g. `1` would be converted to `testing::Eq(1)`).
  //
  // `matcher` must resolve or convert to type `Matcher<R>`.
  template <typename V, typename R, typename M>
    requires internal::IsView<V>
  [[nodiscard]] static StepBuilder CheckViewProperty(ElementSpecifier view,
                                                     R (V::*property)() const,
                                                     M&& matcher);

  // Adds a step that waits for `property` to match `matcher` on `view`. The
  // `add_listener` method must be specified in this version of the function.
  //
  // `V` is the View class, `R` is the type of the property being checked, and
  // `M` is the type of the matcher, or the constant that will be converted to
  // an equality matcher (e.g. `1` would be converted to `testing::Eq(1)`).
  //
  // NOTE: Prefer using the `WaitForViewProperty` macro as it is more concise
  // and creates its own unique event (so you don't have to specify one). For
  // example, the following are equivalent:
  // ```
  //   WaitForViewPropertyCallback(
  //       kMyViewId,
  //       &View::GetEnabled,
  //       &View::AddEnabledChangedListener,
  //       true,
  //       kMyCustomEventType)
  // ```
  // and:
  // ```
  //   WaitForViewProperty(kMyViewId, View, Enabled, true)
  // ```
  //
  // Usage notes:
  // - Do not use with the "Visible" property; use `WaitForShow()` and
  //   `WaitForHide()` instead.
  // - Specify a unique `event` to avoid collisions between parallel or
  //   subsequent wait steps.
  template <typename V, typename R, typename M>
    requires internal::IsView<V>
  [[nodiscard]] static MultiStep WaitForViewPropertyCallback(
      ElementSpecifier view,
      R (V::*property)() const,
      base::CallbackListSubscription (V::*add_listener)(
          ui::metadata::PropertyChangedCallback),
      M&& matcher,
      ui::CustomElementEventType event);

  // Creates a state observer with `id` which polls the view in the current
  // context with `view_id`. If the view is present, uses `callback` to update
  // the state value; otherwise the value is `std::nullopt` (the actual state
  // value is of type `std::optional<T>`).
  //
  // The element, if present, must resolve to a View of the correct type, or the
  // test will fail.
  //
  // See `PollState()` and `PollElement()` for usage details and caveats.
  // Specifically be aware that polling may miss a transient state; prefer to
  // send a custom event or use `WaitForViewPropertyCallback()` if possible.
  template <typename T, typename V, typename C>
    requires internal::IsView<V> &&
             ui::test::internal::HasSignature<C, T(const V*)>
  [[nodiscard]] StepBuilder PollView(
      ui::test::StateIdentifier<PollingViewObserver<T, V>> id,
      ui::ElementIdentifier view_id,
      C&& callback,
      base::TimeDelta polling_interval = ui::test::PollingStateObserver<
          std::optional<T>>::kDefaultPollingInterval);

  // Creates a state observer with `id` which polls `property` on the view in
  // the current context with `view_id`. If the view is not present, the state
  // value will be set to `std::nullopt` (the actual state value is of type
  // `std::optional<T>`).
  //
  // The element, if present, must resolve to a View of the correct type, or the
  // test will fail.
  //
  // See `PollState()` and `PollElement()` for usage details and caveats.
  // Specifically be aware that polling may miss a transient state; prefer to
  // send a custom event or use `WaitForViewPropertyCallback()` if possible.
  template <typename R, typename V, typename T = std::remove_cvref_t<R>>
    requires internal::IsView<V>
  [[nodiscard]] StepBuilder PollViewProperty(
      ui::test::StateIdentifier<PollingViewPropertyObserver<T, V>> id,
      ui::ElementIdentifier view_id,
      R (V::*property)() const,
      base::TimeDelta polling_interval = ui::test::PollingStateObserver<
          std::optional<T>>::kDefaultPollingInterval);

  // Scrolls `view` into the visible viewport if it is currently scrolled
  // outside its container. The view must be otherwise present and visible.
  // Has no effect if the view is not in a scroll container.
  [[nodiscard]] static StepBuilder ScrollIntoView(ElementSpecifier view);

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

  // As IfElement(), but `condition` takes a single argument that is a const
  // View pointer. If `element` is not a view of type V, then the test will
  // fail.
  template <typename C, typename V = internal::ViewArgType<0, C>>
    requires ui::test::internal::HasSignature<
        C,
        bool(const V*)>  // NOLINT(readability/casting)
  [[nodiscard]] static StepBuilder IfView(ElementSpecifier element,
                                          C&& condition,
                                          ThenBlock then_steps,
                                          ElseBlock else_steps = Else());

  // As IfElementMatches(), but `function` takes a single argument that is a
  // const View pointer. If `element` is not a view of type V, then the test
  // will fail.
  template <typename F,
            typename M,
            typename R = ui::test::internal::ReturnTypeOf<F>,
            typename V = internal::ViewArgType<0, F>>
    requires ui::test::internal::HasSignature<F, R(const V*)>
  [[nodiscard]] static StepBuilder IfViewMatches(ElementSpecifier element,
                                                 F&& function,
                                                 M&& matcher,
                                                 ThenBlock then_steps,
                                                 ElseBlock else_steps = Else());

  // Executes `then_steps` if `property` of the view `element` (which must be of
  // the correct View type) matches `matcher`, otherwise executes `else_steps`.
  //
  // Note that bare literal strings cannot be passed as `matcher` for properties
  // with string values, you will need to either explicitly pass a
  // std::[u16]string or explicitly construct a testing::Eq matcher.
  template <typename R, typename M, typename V>
    requires internal::IsView<V>
  [[nodiscard]] static StepBuilder IfViewPropertyMatches(
      ElementSpecifier element,
      R (V::*property)() const,
      M&& matcher,
      ThenBlock then_steps,
      ElseBlock else_steps = Else());

  // On some platforms, context menu operations run in an OS message pump that
  // ignores non-input events, so async Kombucha does not work, as the posted
  // tasks won't be run.
  //
  // Wrap any context menu operation (including the triggering event, if it is a
  // `ClickMouse(ui_controls::RIGHT)`) up to and including the step that closes
  // the context menu in this modifier. If your test fails to close the context
  // menu, it may hang, as there is no single automated way to clean up context
  // menus in Views.
  template <typename... Args>
  [[nodiscard]] static MultiStep MayInvolveNativeContextMenu(Args&&... args) {
#if BUILDFLAG(IS_MAC)
    return WithoutDelay(std::forward<Args>(args)...);
#else
    return Steps(std::forward<Args>(args)...);
#endif
  }

  // Sets the context widget. Must be called before RunTestSequence() or any of
  // the mouse functions.
  void SetContextWidget(Widget* context_widget);
  Widget* context_widget() { return context_widget_.get(); }

 private:
  using FindViewCallback = base::OnceCallback<View*(View*)>;
  static FindViewCallback GetFindViewCallback(AbsoluteViewSpecifier spec);
  static FindViewCallback GetFindViewCallback(ChildViewSpecifier spec);

  // Recursively finds an element that matches `matcher` starting with (but
  // not including) `from`. If `recursive` is true, searches all descendants,
  // otherwise searches children.
  static views::View* FindMatchingView(const views::View* from,
                                       ViewMatcher& matcher,
                                       bool recursive);

  // Converts a *PositionSpecifier to an appropriate *PositionCallback.
  static RelativePositionCallback GetPositionCallback(
      AbsolutePositionSpecifier spec);
  static RelativePositionCallback GetPositionCallback(
      RelativePositionSpecifier spec);

  // Creates the follow-up step for a mouse action.
  StepBuilder CreateMouseFollowUpStep(std::string_view description);

  const raw_ptr<internal::InteractiveViewsTestPrivate> test_impl_;

  base::WeakPtr<Widget> context_widget_;
};

// Template that adds InteractiveViewsTestApi to any test fixture.
//
// You must call SetContextWidget() before using RunTestSequence() or any of the
// mouse actions.
//
// See //chrome/test/interaction/README.md for usage.
template <typename T>
class InteractiveViewsTestMixin : public T, public InteractiveViewsTestApi {
 public:
  template <typename... Args>
  explicit InteractiveViewsTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~InteractiveViewsTestMixin() override = default;

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

// Template definitions:

// static
template <class T>
T* InteractiveViewsTestApi::AsView(ui::TrackedElement* el) {
  auto* const views_el = el->AsA<TrackedElementViews>();
  CHECK(views_el);
  T* const view = AsViewClass<T>(views_el->view());
  CHECK(view);
  return view;
}

// static
template <class T>
const T* InteractiveViewsTestApi::AsView(const ui::TrackedElement* el) {
  const auto* const views_el = el->AsA<TrackedElementViews>();
  CHECK(views_el);
  const T* const view = AsViewClass<T>(views_el->view());
  CHECK(view);
  return view;
}

// static
template <typename C, typename V, typename R>
  requires ui::test::internal::HasSignature<C, R*(V*)>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::NameViewRelative(
    ElementSpecifier relative_to,
    std::string_view name,
    C&& find_callback) {
  StepBuilder builder;
  builder.SetDescription(
      base::StringPrintf("NameViewRelative( \"%s\" )", name.data()));
  builder.SetElement(relative_to);
  builder.SetMustBeVisibleAtStart(true);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<R*(V*)> find_callback, std::string name,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        V* relative_to = nullptr;
        if (el->identifier() !=
            ui::test::internal::kInteractiveTestPivotElementId) {
          if (!el->IsA<TrackedElementViews>()) {
            LOG(ERROR) << "NameView(): Target element is not a View.";
            seq->FailForTesting();
            return;
          }
          View* const view = el->AsA<TrackedElementViews>()->view();
          if (!IsViewClass<V>(view)) {
            LOG(ERROR) << "NameView(): Target View is of type "
                       << view->GetClassName() << " but expected "
                       << V::kViewClassName;
            seq->FailForTesting();
            return;
          }
          relative_to = AsViewClass<V>(view);
        }
        View* const result = std::move(find_callback).Run(relative_to);
        if (!result) {
          LOG(ERROR) << "NameView(): No View found.";
          seq->FailForTesting();
          return;
        }
        auto* const target_element =
            ElementTrackerViews::GetInstance()->GetElementForView(
                result, /* assign_temporary_id =*/true);
        if (!target_element) {
          LOG(ERROR)
              << "NameView(): attempting to name View that is not visible.";
          seq->FailForTesting();
          return;
        }
        seq->NameElement(target_element, name);
      },
      ui::test::internal::MaybeBind(std::forward<C>(find_callback)),
      std::string(name)));
  return builder;
}

// static
template <typename F, typename V>
  requires ui::test::internal::HasSignature<F, void(V*)>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::WithView(
    ElementSpecifier view,
    F&& function) {
  StepBuilder builder;
  builder.SetDescription("WithView()");
  builder.SetElement(view);
  builder.SetMustBeVisibleAtStart(true);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<void(V*)> function, ui::InteractionSequence* seq,
         ui::TrackedElement* el) { std::move(function).Run(AsView<V>(el)); },
      ui::test::internal::MaybeBind(std::forward<F>(function))));
  return builder;
}

// static
template <typename C, typename V>
  requires ui::test::internal::HasSignature<
      C,
      bool(const V*)>  // NOLINT(readability/casting)
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::IfView(
    ElementSpecifier element,
    C&& condition,
    ThenBlock then_steps,
    ElseBlock else_steps) {
  return std::move(
      IfElement(element,
                base::BindOnce(
                    [](base::OnceCallback<bool(const V*)> condition,
                       const ui::InteractionSequence* seq,
                       const ui::TrackedElement* el) {
                      const V* const view = el ? AsView<V>(el) : nullptr;
                      return std::move(condition).Run(view);
                    },
                    ui::test::internal::MaybeBind(std::forward<C>(condition))),
                std::move(then_steps), std::move(else_steps))
          .SetDescription("IfView()"));
}

// static
template <typename F, typename M, typename R, typename V>
  requires ui::test::internal::HasSignature<F, R(const V*)>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::IfViewMatches(
    ElementSpecifier element,
    F&& function,
    M&& matcher,
    ThenBlock then_steps,
    ElseBlock else_steps) {
  return std::move(
      IfElementMatches(
          element,
          base::BindOnce(
              [](base::OnceCallback<R(const V*)> condition,
                 const ui::InteractionSequence* seq,
                 const ui::TrackedElement* el) {
                const V* const view = el ? AsView<V>(el) : nullptr;
                return std::move(condition).Run(view);
              },
              ui::test::internal::MaybeBind(std::forward<F>(function))),
          testing::Matcher<ui::test::internal::MatcherTypeFor<R>>(
              std::forward<M>(matcher)),
          std::move(then_steps), std::move(else_steps))
          .SetDescription("IfViewMatches()"));
}

// static
template <typename R, typename M, typename V>
  requires internal::IsView<V>
ui::InteractionSequence::StepBuilder
InteractiveViewsTestApi::IfViewPropertyMatches(ElementSpecifier element,
                                               R (V::*property)() const,
                                               M&& matcher,
                                               ThenBlock then_steps,
                                               ElseBlock else_steps) {
  using Return = std::remove_cvref_t<R>;
  base::OnceCallback<Return(const V*)> function = base::BindOnce(
      [](R (V::*property)() const, const V* view) -> Return {
        return (view->*property)();
      },
      std::move(property));
  return std::move(IfViewMatches(element, std::move(function),
                                 std::forward<M>(matcher),
                                 std::move(then_steps), std::move(else_steps))
                       .SetDescription("IfViewPropertyMatches()"));
}

// static
template <typename V>
  requires internal::IsView<V>
ui::InteractionSequence::StepBuilder
InteractiveViewsTestApi::NameChildViewByType(ElementSpecifier parent,
                                             std::string_view name,
                                             size_t index) {
  return std::move(NameChildView(parent, name,
                                 base::BindRepeating(
                                     [](size_t& index, const View* view) {
                                       if (IsViewClass<V>(view)) {
                                         if (index == 0) {
                                           return true;
                                         }
                                         --index;
                                       }
                                       return false;
                                     },
                                     base::OwnedRef(index)))
                       .SetDescription(base::StringPrintf(
                           "NameChildViewByType<%s>( \"%s\" %zu )",
                           V::kViewClassName, name.data(), index)));
}

// static
template <typename V>
  requires internal::IsView<V>
ui::InteractionSequence::StepBuilder
InteractiveViewsTestApi::NameDescendantViewByType(ElementSpecifier ancestor,
                                                  std::string_view name,
                                                  size_t index) {
  return std::move(NameDescendantView(ancestor, name,
                                      base::BindRepeating(
                                          [](size_t& index, const View* view) {
                                            if (IsViewClass<V>(view)) {
                                              if (index == 0) {
                                                return true;
                                              }
                                              --index;
                                            }
                                            return false;
                                          },
                                          base::OwnedRef(index)))
                       .SetDescription(base::StringPrintf(
                           "NameDescendantViewByType<%s>( \"%s\" %zu )",
                           V::kViewClassName, name.data(), index)));
}

// static
template <typename F, typename V>
// NOLINTNEXTLINE(readability/casting)
  requires ui::test::internal::HasSignature<F, bool(V*)>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::CheckView(
    ElementSpecifier view,
    F&& check) {
  return CheckView(view, std::forward<F>(check), true);
}

// static
template <typename F, typename M, typename R, typename V>
  requires ui::test::internal::HasSignature<F, R(V*)>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::CheckView(
    ElementSpecifier view,
    F&& function,
    M&& matcher) {
  StepBuilder builder;
  builder.SetDescription("CheckView()");
  builder.SetElement(view);
  using MatcherType = ui::test::internal::MatcherTypeFor<R>;
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<R(V*)> function,
         testing::Matcher<MatcherType> matcher,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        if (!ui::test::internal::MatchAndExplain(
                "CheckView()", matcher,
                MatcherType(std::move(function).Run(AsView<V>(el))))) {
          seq->FailForTesting();
        }
      },
      ui::test::internal::MaybeBind(std::forward<F>(function)),
      testing::Matcher<MatcherType>(std::forward<M>(matcher))));
  return builder;
}

// static
template <typename V, typename R, typename M>
  requires internal::IsView<V>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::CheckViewProperty(
    ElementSpecifier view,
    R (V::*property)() const,
    M&& matcher) {
  StepBuilder builder;
  builder.SetDescription("CheckViewProperty()");
  builder.SetElement(view);
  using MatcherType = ui::test::internal::MatcherTypeFor<R>;
  builder.SetStartCallback(base::BindOnce(
      [](R (V::*property)() const, testing::Matcher<MatcherType> matcher,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        if (!ui::test::internal::MatchAndExplain(
                "CheckViewProperty()", matcher,
                MatcherType((AsView<V>(el)->*property)()))) {
          seq->FailForTesting();
        }
      },
      property, testing::Matcher<MatcherType>(std::forward<M>(matcher))));
  return builder;
}

// static
template <typename V, typename R, typename M>
  requires internal::IsView<V>
ui::test::InteractiveTestApi::MultiStep
InteractiveViewsTestApi::WaitForViewPropertyCallback(
    ElementSpecifier view,
    R (V::*property)() const,
    base::CallbackListSubscription (V::*add_listener)(
        ui::metadata::PropertyChangedCallback),
    M&& matcher,
    ui::CustomElementEventType event_type) {
  // Need to make this ref-counted to ensure it lives long enough to actually
  // listen for the state change.
  using RefCountedSubscription =
      scoped_refptr<base::RefCountedData<base::CallbackListSubscription>>;
  RefCountedSubscription subscription =
      base::MakeRefCounted<RefCountedSubscription::element_type>();

  // The first step will check the property, and either immediately send the
  // event or install the observer that will send the event when the state
  // achieves the correct value.
  using MatcherType = ui::test::internal::MatcherTypeFor<R>;
  auto observe_property = base::BindOnce(
      [](RefCountedSubscription subscription, R (V::*property)() const,
         base::CallbackListSubscription (V::*add_listener)(
             ui::metadata::PropertyChangedCallback),
         ui::CustomElementEventType event_type,
         testing::Matcher<MatcherType> matcher,
         ui::TrackedElement* el) {
        auto* const view = AsView<V>(el);
        if (matcher.Matches(MatcherType((view->*property)()))) {
          // Property is already in the desired state, send event immediately.
          ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
              el, event_type);
        } else {
          // Watch the property for a value that satisfies the matcher.
          subscription->data = (view->*add_listener)(base::BindRepeating(
              [](V* view, R (V::*property)() const,
                 ui::CustomElementEventType event_type,
                 testing::Matcher<MatcherType> matcher) {
                if (matcher.Matches(MatcherType((view->*property)()))) {
                  ElementTrackerViews::GetInstance()->NotifyCustomEvent(
                      event_type, view);
                }
              },
              view, property, event_type, std::move(matcher)));
        }
      },
      subscription, property, add_listener, event_type,
      testing::Matcher<MatcherType>(std::forward<M>(matcher)));

  auto steps = Steps(
      AfterShow(view, std::move(observe_property)).SetMustRemainVisible(true),
      AfterEvent(view, event_type, [subscription]() {
        // Need to reference subscription by value so that it is
        // not discarded until this step runs or the sequence
        // fails.
        subscription->data = base::CallbackListSubscription();
      }));
  AddDescriptionPrefix(
      steps, base::StrCat({"WaitForProperty( ", event_type.GetName(), ", )"}));
  return steps;
}

// Waits for a property named `Property` to have a value that matches `matcher`
// on View `view` of View class `Class`. Convenience method for
// `InteractiveViewsTestApi::WaitForClassPropertyCallback`.
//
// Do not use with the "Visible" property; use `WaitForShow()` or
// `WaitForHide()` instead.
#define WaitForViewProperty(view, Class, Property, matcher)                    \
  []() {                                                                       \
    DEFINE_MACRO_CUSTOM_ELEMENT_EVENT_TYPE(__FILE__, __LINE__,                 \
                                           kWaitFor##Property##Event);         \
    return WaitForViewPropertyCallback((view), &Class::Get##Property,          \
                                       &Class::Add##Property##ChangedCallback, \
                                       (matcher), kWaitFor##Property##Event);  \
  }()

template <typename T, typename V, typename C>
  requires internal::IsView<V> &&
           ui::test::internal::HasSignature<C, T(const V*)>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::PollView(
    ui::test::StateIdentifier<PollingViewObserver<T, V>> id,
    ui::ElementIdentifier view_id,
    C&& callback,
    base::TimeDelta polling_interval) {
  using Cb = PollingViewObserver<T, V>::PollViewCallback;
  Cb cb = ui::test::internal::MaybeBindRepeating(std::forward<C>(callback));
  auto step =
      WithElement(ui::test::internal::kInteractiveTestPivotElementId,
                  base::BindOnce(
                      [](InteractiveViewsTestApi* api, ui::ElementIdentifier id,
                         ui::ElementIdentifier view_id, Cb callback,
                         base::TimeDelta polling_interval,
                         ui::InteractionSequence* seq, ui::TrackedElement* el) {
                        api->private_test_impl().AddStateObserver(
                            id, el->context(),
                            std::make_unique<PollingViewObserver<T, V>>(
                                view_id,
                                seq->IsCurrentStepInAnyContextForTesting()
                                    ? std::nullopt
                                    : std::make_optional(el->context()),
                                std::move(callback), polling_interval));
                      },
                      base::Unretained(this), id.identifier(), view_id, cb,
                      polling_interval));
  step.SetDescription(
      base::StringPrintf("PollView(%s)", view_id.GetName().c_str()));
  return step;
}

template <typename R, typename V, typename T>
  requires internal::IsView<V>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::PollViewProperty(
    ui::test::StateIdentifier<PollingViewPropertyObserver<T, V>> id,
    ui::ElementIdentifier view_id,
    R (V::*property)() const,
    base::TimeDelta polling_interval) {
  auto step = WithElement(
      ui::test::internal::kInteractiveTestPivotElementId,
      base::BindOnce(
          [](InteractiveViewsTestApi* api, ui::ElementIdentifier id,
             ui::ElementIdentifier view_id, R (V::*property)() const,
             base::TimeDelta polling_interval, ui::InteractionSequence* seq,
             ui::TrackedElement* el) {
            api->private_test_impl().AddStateObserver(
                id, el->context(),
                std::make_unique<PollingViewPropertyObserver<T, V>>(
                    view_id,
                    seq->IsCurrentStepInAnyContextForTesting()
                        ? std::nullopt
                        : std::make_optional(el->context()),
                    property, polling_interval));
          },
          base::Unretained(this), id.identifier(), view_id, property,
          polling_interval));
  step.SetDescription(
      base::StringPrintf("PollViewProperty(%s)", view_id.GetName().c_str()));
  return step;
}

}  // namespace views::test
#endif  // UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_H_
