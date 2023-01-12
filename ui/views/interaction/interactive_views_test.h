// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_H_
#define UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_mouse.h"
#include "ui/views/interaction/interactive_views_test_internal.h"
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
// This class is not a test fixture; your test fixture can inherit from it to
// import all of the test API it provides. You will need to call
// private_test_impl().DoTestSetUp() in your SetUp() method and
// private_test_impl().DoTestTearDown() in your TearDown() method and you must
// call SetContextWidget() before running your test sequence. For this reason,
// we provide a convenience class, InteractiveViewsTest, below, which is
// pre-configured to handle all of this for you.
//
// To use Kombucha for in-process browser tests, instead see:
// //chrome/test/interaction/interactive_browser_test.h
class InteractiveViewsTestApi : public ui::test::InteractiveTestApi {
 public:
  InteractiveViewsTestApi();
  ~InteractiveViewsTestApi() override;

  // Returns an object that can be used to inject mouse inputs. Generally,
  // prefer to use methods like MoveMouseTo, MouseClick, and DragMouseTo.
  InteractionTestUtilMouse& mouse_util() { return test_impl().mouse_util(); }

  // Shorthand to convert a tracked element into a View. The element should be
  // a views::TrackedElementViews and of type `T`.
  template <typename T = View>
  static T* AsView(ui::TrackedElement* el);

  // Runs a test InteractionSequence from a series of Steps or StepBuilders with
  // RunSynchronouslyForTesting(). Hooks both the completed and aborted
  // callbacks to ensure completion, and prints an error on failure. The context
  // will be pulled from `context_widget()`.
  template <typename... Args>
  bool RunTestSequence(Args&&... steps);

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
  using AbsoluteViewSpecifier = absl::variant<
      // Specify a view that is known at the time the sequence is created. The
      // View must persist until the step executes.
      View*,
      // Specify a view that will be valid by the time the step executes (i.e.
      // is set in a previous step callback) but not at the time the test
      // sequence is built. The view will be read from the target variable,
      // which must point to a valid view.
      View**,
      // Find and return a view based on an arbitrary rule.
      base::OnceCallback<View*()>>;

  // Specifies a view relative to its parent.
  using ChildViewSpecifier = absl::variant<
      // The index of the child in the parent view. An out of bounds index will
      // generate an error.
      size_t,
      // Specifies a filter that is applied to the children; the first child
      // view to satisfy the filter (i.e. return true) is named.
      ViewMatcher>;

  // Specifies a view relative to another view `relative_to` based on an
  // arbitrary rule. The resulting view does not need to be a descendant (or
  // even an ancestor) of `relative_to`. Your callback may take any type of View
  // as an argument, if the target element cannot be properly converted to that
  // type, the step will fail.
  template <typename V, template <typename...> typename C = base::OnceCallback>
  using FindViewCallback = C<View*(V* relative_to)>;

  // Methods that name views.

  template <typename V, template <typename...> typename C = base::OnceCallback>
  [[nodiscard]] static StepBuilder NameViewRelative(
      ElementSpecifier relative_to,
      base::StringPiece name,
      FindViewCallback<V, C> find_callback);

  [[nodiscard]] static StepBuilder NameView(base::StringPiece name,
                                            AbsoluteViewSpecifier spec);

  [[nodiscard]] static StepBuilder NameChildView(ElementSpecifier parent,
                                                 base::StringPiece name,
                                                 ChildViewSpecifier spec);

  [[nodiscard]] static StepBuilder NameDescendantView(ElementSpecifier ancestor,
                                                      base::StringPiece name,
                                                      ViewMatcher matcher);

  // Names the `index` (0-indexed) child view of `parent` that is of type `V`.
  template <typename V>
  [[nodiscard]] static StepBuilder NameChildViewByType(ElementSpecifier parent,
                                                       base::StringPiece name,
                                                       size_t index = 0);

  // Names the `index` (0-indexed) descendant view of `parent` in depth-first
  // traversal order that is of type `V`.
  template <typename V>
  [[nodiscard]] static StepBuilder NameDescendantViewByType(
      ElementSpecifier ancestor,
      base::StringPiece name,
      size_t index = 0);

  // As WithElement, but `view` should resolve to a TrackedElementViews wrapping
  // a view of type `V`.
  template <template <typename...> typename C, typename V>
  [[nodiscard]] static StepBuilder WithView(ElementSpecifier view,
                                            C<void(V*)> function);

  // As CheckElement(), but `view` should resolve to a TrackedElementViews
  // wrapping a view of type `V`.
  template <typename V>
  [[nodiscard]] static StepBuilder CheckView(
      ElementSpecifier view,
      base::OnceCallback<bool(V* view)> check);

  // As CheckView(), but checks that the result of calling `function` on `view`
  // matches `matcher`. If not, the mismatch is printed and the test fails.
  template <template <typename...> typename C,
            typename V,
            typename T,
            typename U>
  [[nodiscard]] static StepBuilder CheckView(ElementSpecifier view,
                                             C<T(V*)> function,
                                             U&& matcher);

  // As CheckView() but checks that `matcher` matches the value returned by
  // calling `property` on `view`. On failure, logs the matcher error and fails
  // the test.
  template <typename V, typename T, typename U>
  [[nodiscard]] static StepBuilder CheckViewProperty(ElementSpecifier view,
                                                     T (V::*property)() const,
                                                     U&& matcher);

  // Indicates that the center point of the target element should be used for a
  // mouse move.
  struct CenterPoint {};

  // Function that returns a destination for a move or drag.
  using AbsolutePositionCallback = base::OnceCallback<gfx::Point()>;

  // Specifies an absolute position for a mouse move or drag that does not need
  // a reference element.
  using AbsolutePositionSpecifier = absl::variant<
      // Use this specific position. This value is stored when the sequence is
      // created; use gfx::Point* if you want to capture a point during sequence
      // execution.
      gfx::Point,
      // As above, but the position is read from the memory address on execution
      // instead of copied when the test sequence is constructed. Use this when
      // you want to calculate and cache a point during test execution for later
      // use. The pointer must remain valid through the end of the test.
      gfx::Point*,
      // Use the return value of the supplied callback
      AbsolutePositionCallback>;

  // Specifies how the `reference_element` should be used (or not) to generate a
  // target point for a mouse move.
  using RelativePositionCallback =
      base::OnceCallback<gfx::Point(ui::TrackedElement* reference_element)>;

  // Specifies how the target position of a mouse operation (in screen
  // coordinates) will be determined.
  using RelativePositionSpecifier = absl::variant<
      // Default to the centerpoint of the reference element, which should be a
      // views::View.
      CenterPoint,
      // Use the return value of the supplied callback.
      RelativePositionCallback>;

  // Move the mouse to the specified `position` in screen coordinates. The
  // `reference` element will be used based on how `position` is specified.
  [[nodiscard]] StepBuilder MoveMouseTo(AbsolutePositionSpecifier position);
  [[nodiscard]] StepBuilder MoveMouseTo(
      ElementSpecifier reference,
      RelativePositionSpecifier position = CenterPoint());

  // Clicks mouse button `button` at the current cursor position.
  [[nodiscard]] StepBuilder ClickMouse(
      ui_controls::MouseButton button = ui_controls::LEFT,
      bool release = true);

  // Depresses the left mouse button at the current cursor position and drags to
  // the target `position`. The `reference` element will be used based on how
  // `position` is specified.
  [[nodiscard]] StepBuilder DragMouseTo(AbsolutePositionSpecifier position,
                                        bool release = true);
  [[nodiscard]] StepBuilder DragMouseTo(
      ElementSpecifier reference,
      RelativePositionSpecifier position = CenterPoint(),
      bool release = true);

  // Releases the specified mouse button. Use when you previously called
  // ClickMouse() or DragMouseTo() with `release` = false.
  [[nodiscard]] StepBuilder ReleaseMouse(
      ui_controls::MouseButton button = ui_controls::LEFT);

  // Sets the context widget. Must be called before RunTestSequence() or any of
  // the mouse functions.
  void SetContextWidget(Widget* context_widget);
  Widget* context_widget() { return context_widget_; }

 protected:
  explicit InteractiveViewsTestApi(
      std::unique_ptr<internal::InteractiveViewsTestPrivate> private_test_impl);

 private:
  static FindViewCallback<View> GetFindViewCallback(AbsoluteViewSpecifier spec);
  static FindViewCallback<View> GetFindViewCallback(ChildViewSpecifier spec);

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

  internal::InteractiveViewsTestPrivate& test_impl() {
    return static_cast<internal::InteractiveViewsTestPrivate&>(
        InteractiveTestApi::private_test_impl());
  }

  // Creates the follow-up step for a mouse action.
  StepBuilder CreateMouseFollowUpStep(const base::StringPiece& description);

  base::raw_ptr<Widget, DanglingUntriaged> context_widget_ = nullptr;
};

// Test fixture for Views tests that supports the InteractiveViewsTestApi
// convenience methods.
//
// You must call SetContextWidget() before using RunTestSequence() or any of the
// mouse actions.
//
// See //chrome/test/interaction/README.md for usage.
class InteractiveViewsTest : public ViewsTestBase,
                             public InteractiveViewsTestApi {
 public:
  // Constructs a ViewsTestBase with |traits| being forwarded to its
  // TaskEnvironment. MainThreadType always defaults to UI and must not be
  // specified.
  template <typename... TaskEnvironmentTraits>
  NOINLINE explicit InteractiveViewsTest(TaskEnvironmentTraits&&... traits)
      : ViewsTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}

  explicit InteractiveViewsTest(
      std::unique_ptr<base::test::TaskEnvironment> task_environment);

  ~InteractiveViewsTest() override;

  void SetUp() override;
  void TearDown() override;
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

template <typename... Args>
bool InteractiveViewsTestApi::RunTestSequence(Args&&... steps) {
  return RunTestSequenceInContext(
      ElementTrackerViews::GetContextForWidget(context_widget()),
      std::forward<Args>(steps)...);
}

// static
template <typename V, template <typename...> typename C>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::NameViewRelative(
    ElementSpecifier relative_to,
    base::StringPiece name,
    FindViewCallback<V, C> find_callback) {
  StepBuilder builder;
  builder.SetDescription(
      base::StringPrintf("NameViewRelative( \"%s\" )", name.data()));
  ui::test::internal::SpecifyElement(builder, relative_to);
  builder.SetMustBeVisibleAtStart(true);
  builder.SetStartCallback(base::BindOnce(
      [](FindViewCallback<V, C> find_callback, std::string name,
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
                       << V::MetaData()->type_name();
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
      std::move(find_callback), std::string(name)));
  return builder;
}

// static
template <template <typename...> typename C, typename V>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::WithView(
    ElementSpecifier view,
    C<void(V*)> function) {
  StepBuilder builder;
  builder.SetDescription("WithView()");
  ui::test::internal::SpecifyElement(builder, view);
  builder.SetMustBeVisibleAtStart(true);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<void(V*)> function, ui::InteractionSequence* seq,
         ui::TrackedElement* el) { std::move(function).Run(AsView<V>(el)); },
      base::OnceCallback<void(V*)>(std::move(function))));
  return builder;
}

// static
template <typename V>
ui::InteractionSequence::StepBuilder
InteractiveViewsTestApi::NameChildViewByType(ElementSpecifier parent,
                                             base::StringPiece name,
                                             size_t index) {
  return std::move(
      NameChildView(parent, name,
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
              V::MetaData()->type_name().c_str(), name.data(), index)));
}

// static
template <typename V>
ui::InteractionSequence::StepBuilder
InteractiveViewsTestApi::NameDescendantViewByType(ElementSpecifier ancestor,
                                                  base::StringPiece name,
                                                  size_t index) {
  return std::move(
      NameDescendantView(ancestor, name,
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
              V::MetaData()->type_name().c_str(), name.data(), index)));
}

// static
template <typename V>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::CheckView(
    ElementSpecifier view,
    base::OnceCallback<bool(V* view)> check) {
  return CheckView(view, std::move(check), true);
}

// static
template <template <typename...> typename C, typename V, typename T, typename U>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::CheckView(
    ElementSpecifier view,
    C<T(V*)> function,
    U&& matcher) {
  StepBuilder builder;
  builder.SetDescription("CheckView()");
  ui::test::internal::SpecifyElement(builder, view);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<T(V*)> function, testing::Matcher<T> matcher,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        if (!ui::test::internal::MatchAndExplain(
                "CheckView()", matcher,
                std::move(function).Run(AsView<V>(el)))) {
          seq->FailForTesting();
        }
      },
      base::OnceCallback<T(V*)>(std::move(function)),
      testing::Matcher<T>(std::forward<U>(matcher))));
  return builder;
}

// static
template <typename V, typename T, typename U>
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::CheckViewProperty(
    ElementSpecifier view,
    T (V::*property)() const,
    U&& matcher) {
  StepBuilder builder;
  builder.SetDescription("CheckViewProperty()");
  ui::test::internal::SpecifyElement(builder, view);
  builder.SetStartCallback(base::BindOnce(
      [](T (V::*property)() const, testing::Matcher<T> matcher,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        if (!ui::test::internal::MatchAndExplain(
                "CheckViewProperty()", matcher, (AsView<V>(el)->*property)())) {
          seq->FailForTesting();
        }
      },
      property, testing::Matcher<T>(std::forward<U>(matcher))));
  return builder;
}

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_H_
