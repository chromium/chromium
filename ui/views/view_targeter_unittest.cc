// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_targeter.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_targeter.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/root_view.h"

namespace views {

// A derived class of View used for testing purposes.
class TestingView : public View, public ViewTargeterDelegate {
  METADATA_HEADER(TestingView, View)

 public:
  TestingView() = default;

  TestingView(const TestingView&) = delete;
  TestingView& operator=(const TestingView&) = delete;

  ~TestingView() override = default;

  // Reset all test state.
  void Reset() { SetCanProcessEventsWithinSubtree(true); }

  // A call-through function to ViewTargeterDelegate::DoesIntersectRect().
  bool TestDoesIntersectRect(const View* target, const gfx::Rect& rect) const {
    return DoesIntersectRect(target, rect);
  }
};

BEGIN_METADATA(TestingView)
END_METADATA

// A derived class of View having a triangular-shaped hit test mask.
class TestMaskedView : public View, public MaskedTargeterDelegate {
  METADATA_HEADER(TestMaskedView, View)

 public:
  TestMaskedView() = default;

  TestMaskedView(const TestMaskedView&) = delete;
  TestMaskedView& operator=(const TestMaskedView&) = delete;

  ~TestMaskedView() override = default;

  // A call-through function to MaskedTargeterDelegate::DoesIntersectRect().
  bool TestDoesIntersectRect(const View* target, const gfx::Rect& rect) const {
    return DoesIntersectRect(target, rect);
  }

 private:
  // MaskedTargeterDelegate:
  bool GetHitTestMask(SkPath* mask) const override {
    DCHECK(mask);
    SkScalar w = SkIntToScalar(width());
    SkScalar h = SkIntToScalar(height());

    // Create a triangular mask within the bounds of this View.
    mask->moveTo(w / 2, 0);
    mask->lineTo(w, h);
    mask->lineTo(0, h);
    mask->close();
    return true;
  }
};

BEGIN_METADATA(TestMaskedView)
END_METADATA

namespace test {

// TODO(tdanderson): Clean up this test suite by moving common code/state into
//                   ViewTargeterTest and overriding SetUp(), TearDown(), etc.
//                   See crbug.com/355680.
class ViewTargeterTest : public ViewsTestBase {
 public:
  ViewTargeterTest() = default;

  ViewTargeterTest(const ViewTargeterTest&) = delete;
  ViewTargeterTest& operator=(const ViewTargeterTest&) = delete;

  ~ViewTargeterTest() override = default;

  void SetGestureHandler(internal::RootView* root_view, View* handler) {
    root_view->gesture_handler_ = handler;
  }

  void SetGestureHandlerSetBeforeProcessing(internal::RootView* root_view,
                                            bool set) {
    root_view->gesture_handler_set_before_processing_ = set;
  }
};

namespace {

gfx::Point ConvertPointFromWidgetToView(View* view, const gfx::Point& p) {
  gfx::Point tmp(p);
  View::ConvertPointToTarget(view->GetWidget()->GetRootView(), view, &tmp);
  return tmp;
}

gfx::Rect ConvertRectFromWidgetToView(View* view, const gfx::Rect& r) {
  gfx::Rect tmp(r);
  tmp.set_origin(ConvertPointFromWidgetToView(view, r.origin()));
  return tmp;
}

}  // namespace

// Verifies that the the functions ViewTargeter::FindTargetForEvent()
// and ViewTargeter::FindNextBestTarget() are implemented correctly
// for key events.
TEST_F(ViewTargeterTest, ViewTargeterForKeyEvents) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(init_params));
  widget->Show();

  View* content = widget->SetContentsView(std::make_unique<View>());
  View* child = content->AddChildView(std::make_unique<View>());
  View* grandchild = child->AddChildView(std::make_unique<View>());

  grandchild->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  grandchild->RequestFocus();

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  ui::EventTargeter* targeter = root_view->targeter();

  ui::KeyEvent key_event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);

  // The focused view should be the initial target of the event.
  ui::EventTarget* current_target =
      targeter->FindTargetForEvent(root_view, &key_event);
  EXPECT_EQ(grandchild, static_cast<View*>(current_target));

  // Verify that FindNextBestTarget() will return the parent view of the
  // argument (and NULL if the argument has no parent view).
  current_target = targeter->FindNextBestTarget(grandchild, &key_event);
  EXPECT_EQ(child, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(child, &key_event);
  EXPECT_EQ(content, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(content, &key_event);
  EXPECT_EQ(widget->GetRootView(), static_cast<View*>(current_target));
  current_target =
      targeter->FindNextBestTarget(widget->GetRootView(), &key_event);
  EXPECT_EQ(nullptr, static_cast<View*>(current_target));
}

// Verifies that the the functions ViewTargeter::FindTargetForEvent()
// and ViewTargeter::FindNextBestTarget() are implemented correctly
// for scroll events.
TEST_F(ViewTargeterTest, ViewTargeterForScrollEvents) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget->Init(std::move(init_params));

  // The coordinates used for SetBounds() are in the parent coordinate space.
  auto owning_content = std::make_unique<View>();
  owning_content->SetBounds(0, 0, 100, 100);

  View* content = widget->SetContentsView(std::move(owning_content));
  View* child = content->AddChildView(std::make_unique<View>());
  child->SetBounds(50, 50, 20, 20);
  View* grandchild = child->AddChildView(std::make_unique<View>());
  grandchild->SetBounds(0, 0, 5, 5);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  ui::EventTargeter* targeter = root_view->targeter();

  // The event falls within the bounds of |child| and |content| but not
  // |grandchild|, so |child| should be the initial target for the event.
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(60, 60),
                         ui::EventTimeForNow(), 0, 0, 3, 0, 3, 2);
  ui::EventTarget* current_target =
      targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(child, static_cast<View*>(current_target));

  // Verify that FindNextBestTarget() will return the parent view of the
  // argument (and NULL if the argument has no parent view).
  current_target = targeter->FindNextBestTarget(child, &scroll);
  EXPECT_EQ(content, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(content, &scroll);
  EXPECT_EQ(widget->GetRootView(), static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(widget->GetRootView(), &scroll);
  EXPECT_EQ(nullptr, static_cast<View*>(current_target));

  // The event falls outside of the original specified bounds of |content|,
  // |child|, and |grandchild|. But since |content| is the contents view,
  // and contents views are resized to fill the entire area of the root
  // view, the event's initial target should still be |content|.
  scroll = ui::ScrollEvent(ui::EventType::kScroll, gfx::Point(150, 150),
                           ui::EventTimeForNow(), 0, 0, 3, 0, 3, 2);
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(content, static_cast<View*>(current_target));
}

// Convenience to make constructing a GestureEvent simpler.
ui::GestureEvent CreateTestGestureEvent(
    const ui::GestureEventDetails& details) {
  return ui::GestureEvent(details.bounding_box().CenterPoint().x(),
                          details.bounding_box().CenterPoint().y(), 0,
                          base::TimeTicks(), details);
}

// Verifies that the the functions ViewTargeter::FindTargetForEvent()
// and ViewTargeter::FindNextBestTarget() are implemented correctly
// for gesture events.
TEST_F(ViewTargeterTest, ViewTargeterForGestureEvents) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget->Init(std::move(init_params));

  View* content = widget->SetContentsView(std::make_unique<View>());
  content->SetBounds(0, 0, 100, 100);
  // The coordinates used for SetBounds() are in the parent coordinate space.
  View* child = content->AddChildView(std::make_unique<View>());
  child->SetBounds(50, 50, 20, 20);
  View* grandchild = child->AddChildView(std::make_unique<View>());
  grandchild->SetBounds(0, 0, 5, 5);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  ui::EventTargeter* targeter = root_view->targeter();

  // Define some gesture events for testing.
  gfx::RectF bounding_box(gfx::PointF(46.f, 46.f), gfx::SizeF(8.f, 8.f));
  ui::GestureEventDetails details(ui::EventType::kGestureTap);
  details.set_bounding_box(bounding_box);
  ui::GestureEvent tap = CreateTestGestureEvent(details);
  details = ui::GestureEventDetails(ui::EventType::kGestureScrollBegin);
  details.set_bounding_box(bounding_box);
  ui::GestureEvent scroll_begin = CreateTestGestureEvent(details);
  details = ui::GestureEventDetails(ui::EventType::kGestureEnd);
  details.set_bounding_box(bounding_box);
  ui::GestureEvent end = CreateTestGestureEvent(details);

  // Assume that the view currently handling gestures has been set as
  // |grandchild| by a previous gesture event. Thus subsequent TAP and
  // SCROLL_BEGIN events should be initially targeted to |grandchild|, and
  // re-targeting should be prohibited for TAP but permitted for
  // GESTURE_SCROLL_BEGIN (which should be re-targeted to the parent of
  // |grandchild|).
  SetGestureHandlerSetBeforeProcessing(root_view, true);
  SetGestureHandler(root_view, grandchild);
  EXPECT_EQ(grandchild, targeter->FindTargetForEvent(root_view, &tap));
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(grandchild, &tap));
  EXPECT_EQ(grandchild, targeter->FindTargetForEvent(root_view, &scroll_begin));
  EXPECT_EQ(child, targeter->FindNextBestTarget(grandchild, &scroll_begin));

  // GESTURE_END events should be targeted to the existing gesture handler,
  // but re-targeting should be prohibited.
  EXPECT_EQ(grandchild, targeter->FindTargetForEvent(root_view, &end));
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(grandchild, &end));

  // Assume that the view currently handling gestures is still set as
  // |grandchild|, but this was not done by a previous gesture. Thus we are
  // in the process of finding the View to which subsequent gestures will be
  // dispatched, so TAP and SCROLL_BEGIN events should be re-targeted up
  // the ancestor chain.
  SetGestureHandlerSetBeforeProcessing(root_view, false);
  EXPECT_EQ(child, targeter->FindNextBestTarget(grandchild, &tap));
  EXPECT_EQ(child, targeter->FindNextBestTarget(grandchild, &scroll_begin));

  // GESTURE_END events are not permitted to be re-targeted up the ancestor
  // chain; they are only ever targeted in the case where the gesture handler
  // was established by a previous gesture.
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(grandchild, &end));

  // Assume that the default gesture handler was set by the previous gesture,
  // but that this handler is currently NULL. No gesture events should be
  // re-targeted in this case (regardless of the view that is passed in to
  // FindNextBestTarget() as the previous target).
  SetGestureHandler(root_view, nullptr);
  SetGestureHandlerSetBeforeProcessing(root_view, true);
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(child, &tap));
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(nullptr, &tap));
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(content, &scroll_begin));
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(content, &end));

  // Reset the locations of the gesture events to be in the root view
  // coordinate space since we are about to call FindTargetForEvent()
  // again (calls to FindTargetForEvent() and FindNextBestTarget()
  // mutate the location of the gesture events to be in the coordinate
  // space of the returned view).
  details = ui::GestureEventDetails(ui::EventType::kGestureTap);
  details.set_bounding_box(bounding_box);
  tap = CreateTestGestureEvent(details);
  details = ui::GestureEventDetails(ui::EventType::kGestureScrollBegin);
  details.set_bounding_box(bounding_box);
  scroll_begin = CreateTestGestureEvent(details);
  details = ui::GestureEventDetails(ui::EventType::kGestureEnd);
  details.set_bounding_box(bounding_box);
  end = CreateTestGestureEvent(details);

  // If no default gesture handler is currently set, targeting should be
  // performed using the location of the gesture event for a TAP and a
  // SCROLL_BEGIN.
  SetGestureHandlerSetBeforeProcessing(root_view, false);
  EXPECT_EQ(grandchild, targeter->FindTargetForEvent(root_view, &tap));
  EXPECT_EQ(grandchild, targeter->FindTargetForEvent(root_view, &scroll_begin));

  // If no default gesture handler is currently set, GESTURE_END events
  // should never be re-targeted to any View.
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(nullptr, &end));
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(child, &end));
}

// Tests that the contents view is targeted instead of the root view for
// gesture events that should be targeted to the contents view. Also
// tests that the root view is targeted for gesture events which should
// not be targeted to any other view in the views tree.
TEST_F(ViewTargeterTest, TargetContentsAndRootView) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget->Init(std::move(init_params));

  // The coordinates used for SetBounds() are in the parent coordinate space.
  auto owning_content = std::make_unique<View>();
  owning_content->SetBounds(0, 0, 100, 100);
  View* content = widget->SetContentsView(std::move(owning_content));

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  ui::EventTargeter* targeter = root_view->targeter();

  // A gesture event located entirely within the contents view should
  // target the contents view.
  gfx::RectF bounding_box(gfx::PointF(96.f, 96.f), gfx::SizeF(8.f, 8.f));
  ui::GestureEventDetails details(ui::EventType::kGestureTap);
  details.set_bounding_box(bounding_box);
  ui::GestureEvent tap = CreateTestGestureEvent(details);

  EXPECT_EQ(content, targeter->FindTargetForEvent(root_view, &tap));

  // A gesture event not located entirely within the contents view but
  // having its center within the contents view should target
  // the contents view.
  bounding_box = gfx::RectF(gfx::PointF(194.f, 100.f), gfx::SizeF(8.f, 8.f));
  details.set_bounding_box(bounding_box);
  tap = CreateTestGestureEvent(details);

  EXPECT_EQ(content, targeter->FindTargetForEvent(root_view, &tap));

  // A gesture event with its center not located within the contents
  // view but that overlaps the contents view by at least 60% should
  // target the contents view.
  bounding_box = gfx::RectF(gfx::PointF(50.f, 0.f), gfx::SizeF(400.f, 200.f));
  details.set_bounding_box(bounding_box);
  tap = CreateTestGestureEvent(details);

  EXPECT_EQ(content, targeter->FindTargetForEvent(root_view, &tap));

  // A gesture event not overlapping the contents view by at least
  // 60% and not having its center within the contents view should
  // be targeted to the root view.
  bounding_box = gfx::RectF(gfx::PointF(196.f, 100.f), gfx::SizeF(8.f, 8.f));
  details.set_bounding_box(bounding_box);
  tap = CreateTestGestureEvent(details);

  EXPECT_EQ(widget->GetRootView(),
            targeter->FindTargetForEvent(root_view, &tap));

  // A gesture event completely outside the contents view should be targeted
  // to the root view.
  bounding_box = gfx::RectF(gfx::PointF(205.f, 100.f), gfx::SizeF(8.f, 8.f));
  details.set_bounding_box(bounding_box);
  tap = CreateTestGestureEvent(details);

  EXPECT_EQ(widget->GetRootView(),
            targeter->FindTargetForEvent(root_view, &tap));

  // A gesture event with dimensions 1x1 located entirely within the
  // contents view should target the contents view.
  bounding_box = gfx::RectF(gfx::PointF(175.f, 100.f), gfx::SizeF(1.f, 1.f));
  details.set_bounding_box(bounding_box);
  tap = CreateTestGestureEvent(details);

  EXPECT_EQ(content, targeter->FindTargetForEvent(root_view, &tap));

  // A gesture event with dimensions 1x1 located entirely outside the
  // contents view should be targeted to the root view.
  bounding_box = gfx::RectF(gfx::PointF(205.f, 100.f), gfx::SizeF(1.f, 1.f));
  details.set_bounding_box(bounding_box);
  tap = CreateTestGestureEvent(details);

  EXPECT_EQ(widget->GetRootView(),
            targeter->FindTargetForEvent(root_view, &tap));
}

// Tests that calls to FindTargetForEvent() and FindNextBestTarget() change
// the location of a gesture event to be in the correct coordinate space.
TEST_F(ViewTargeterTest, GestureEventCoordinateConversion) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget->Init(std::move(init_params));

  // The coordinates used for SetBounds() are in the parent coordinate space.
  View* content = widget->SetContentsView(std::make_unique<View>());
  content->SetBounds(0, 0, 100, 100);
  View* child = content->AddChildView(std::make_unique<View>());
  child->SetBounds(50, 50, 20, 20);
  View* grandchild = child->AddChildView(std::make_unique<View>());
  grandchild->SetBounds(5, 5, 10, 10);
  View* great_grandchild = grandchild->AddChildView(std::make_unique<View>());
  great_grandchild->SetBounds(3, 3, 4, 4);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  ui::EventTargeter* targeter = root_view->targeter();

  // Define a GESTURE_TAP event with a bounding box centered at (60, 60)
  // in root view coordinates with width and height of 4.
  gfx::RectF bounding_box(gfx::PointF(58.f, 58.f), gfx::SizeF(4.f, 4.f));
  gfx::PointF center_point(bounding_box.CenterPoint());
  ui::GestureEventDetails details(ui::EventType::kGestureTap);
  details.set_bounding_box(bounding_box);
  ui::GestureEvent tap = CreateTestGestureEvent(details);

  // Calculate the location of the gesture in each of the different
  // coordinate spaces.
  gfx::Point location_in_root(gfx::ToFlooredPoint(center_point));
  EXPECT_EQ(gfx::Point(60, 60), location_in_root);
  gfx::Point location_in_great_grandchild(
      ConvertPointFromWidgetToView(great_grandchild, location_in_root));
  EXPECT_EQ(gfx::Point(2, 2), location_in_great_grandchild);
  gfx::Point location_in_grandchild(
      ConvertPointFromWidgetToView(grandchild, location_in_root));
  EXPECT_EQ(gfx::Point(5, 5), location_in_grandchild);
  gfx::Point location_in_child(
      ConvertPointFromWidgetToView(child, location_in_root));
  EXPECT_EQ(gfx::Point(10, 10), location_in_child);
  gfx::Point location_in_content(
      ConvertPointFromWidgetToView(content, location_in_root));
  EXPECT_EQ(gfx::Point(60, 60), location_in_content);

  // Verify the location of |tap| is in screen coordinates.
  EXPECT_EQ(gfx::Point(60, 60), tap.location());

  // The initial target should be |great_grandchild| and the location of
  // the event should be changed into the coordinate space of the target.
  EXPECT_EQ(great_grandchild, targeter->FindTargetForEvent(root_view, &tap));
  EXPECT_EQ(location_in_great_grandchild, tap.location());
  SetGestureHandler(root_view, great_grandchild);

  // The next target should be |grandchild| and the location of
  // the event should be changed into the coordinate space of the target.
  EXPECT_EQ(grandchild, targeter->FindNextBestTarget(great_grandchild, &tap));
  EXPECT_EQ(location_in_grandchild, tap.location());
  SetGestureHandler(root_view, grandchild);

  // The next target should be |child| and the location of
  // the event should be changed into the coordinate space of the target.
  EXPECT_EQ(child, targeter->FindNextBestTarget(grandchild, &tap));
  EXPECT_EQ(location_in_child, tap.location());
  SetGestureHandler(root_view, child);

  // The next target should be |content| and the location of
  // the event should be changed into the coordinate space of the target.
  EXPECT_EQ(content, targeter->FindNextBestTarget(child, &tap));
  EXPECT_EQ(location_in_content, tap.location());
  SetGestureHandler(root_view, content);

  // The next target should be |root_view| and the location of
  // the event should be changed into the coordinate space of the target.
  EXPECT_EQ(widget->GetRootView(), targeter->FindNextBestTarget(content, &tap));
  EXPECT_EQ(location_in_root, tap.location());
  SetGestureHandler(root_view, widget->GetRootView());

  // The next target should be NULL and the location of the event should
  // remain unchanged.
  EXPECT_EQ(nullptr, targeter->FindNextBestTarget(widget->GetRootView(), &tap));
  EXPECT_EQ(location_in_root, tap.location());
}

// Tests that the functions ViewTargeterDelegate::DoesIntersectRect()
// and MaskedTargeterDelegate::DoesIntersectRect() work as intended when
// called on views which are derived from ViewTargeterDelegate.
// Also verifies that ViewTargeterDelegate::DoesIntersectRect() can
// be called from the ViewTargeter installed on RootView.
TEST_F(ViewTargeterTest, DoesIntersectRect) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(0, 0, 650, 650);
  widget->Init(std::move(params));

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  ViewTargeter* view_targeter = root_view->targeter();

  // The coordinates used for SetBounds() are in the parent coordinate space.
  auto* v1 = root_view->AddChildView(std::make_unique<TestMaskedView>());
  v1->SetBounds(0, 0, 200, 200);
  auto* v2 = root_view->AddChildView(std::make_unique<TestingView>());
  v2->SetBounds(300, 0, 300, 300);
  auto* v3 = v2->AddChildView(std::make_unique<TestMaskedView>());
  v3->SetBounds(0, 0, 100, 100);

  // The coordinates used below are in the local coordinate space of the
  // view that is passed in as an argument.

  // Hit tests against |v1|, which has a hit test mask.
  EXPECT_TRUE(v1->TestDoesIntersectRect(v1, gfx::Rect(0, 0, 200, 200)));
  EXPECT_TRUE(v1->TestDoesIntersectRect(v1, gfx::Rect(-10, -10, 110, 12)));
  EXPECT_TRUE(v1->TestDoesIntersectRect(v1, gfx::Rect(112, 142, 1, 1)));
  EXPECT_FALSE(v1->TestDoesIntersectRect(v1, gfx::Rect(0, 0, 20, 20)));
  EXPECT_FALSE(v1->TestDoesIntersectRect(v1, gfx::Rect(-10, -10, 90, 12)));
  EXPECT_FALSE(v1->TestDoesIntersectRect(v1, gfx::Rect(150, 49, 1, 1)));

  // Hit tests against |v2|, which does not have a hit test mask.
  EXPECT_TRUE(v2->TestDoesIntersectRect(v2, gfx::Rect(0, 0, 200, 200)));
  EXPECT_TRUE(v2->TestDoesIntersectRect(v2, gfx::Rect(-10, 250, 60, 60)));
  EXPECT_TRUE(v2->TestDoesIntersectRect(v2, gfx::Rect(250, 250, 1, 1)));
  EXPECT_FALSE(v2->TestDoesIntersectRect(v2, gfx::Rect(-10, 250, 7, 7)));
  EXPECT_FALSE(v2->TestDoesIntersectRect(v2, gfx::Rect(-1, -1, 1, 1)));

  // Hit tests against |v3|, which has a hit test mask and is a child of |v2|.
  EXPECT_TRUE(v3->TestDoesIntersectRect(v3, gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(v3->TestDoesIntersectRect(v3, gfx::Rect(90, 90, 1, 1)));
  EXPECT_FALSE(v3->TestDoesIntersectRect(v3, gfx::Rect(10, 125, 50, 50)));
  EXPECT_FALSE(v3->TestDoesIntersectRect(v3, gfx::Rect(110, 110, 1, 1)));

  // Verify that hit-testing is performed correctly when using the
  // call-through function ViewTargeter::DoesIntersectRect().
  EXPECT_TRUE(
      view_targeter->DoesIntersectRect(root_view, gfx::Rect(0, 0, 50, 50)));
  EXPECT_FALSE(
      view_targeter->DoesIntersectRect(root_view, gfx::Rect(-20, -20, 10, 10)));
}

// Tests that calls made directly on the hit-testing methods in View
// (HitTestPoint(), HitTestRect(), etc.) return the correct values.
TEST_F(ViewTargeterTest, HitTestCallsOnView) {
  // The coordinates in this test are in the coordinate space of the root view.
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  // |v1| has no hit test mask. No ViewTargeter is installed on |v1|, which
  // means that View::HitTestRect() will call into the targeter installed on
  // the root view instead when we hit test against |v1|.
  gfx::Rect v1_bounds = gfx::Rect(0, 0, 100, 100);
  TestingView* v1 = root_view->AddChildView(std::make_unique<TestingView>());
  v1->SetBoundsRect(v1_bounds);

  // |v2| has a triangular hit test mask. Install a ViewTargeter on |v2| which
  // will be called into by View::HitTestRect().
  gfx::Rect v2_bounds = gfx::Rect(105, 0, 100, 100);
  TestMaskedView* v2 =
      root_view->AddChildView(std::make_unique<TestMaskedView>());
  v2->SetBoundsRect(v2_bounds);
  v2->SetEventTargeter(std::make_unique<ViewTargeter>(v2));

  gfx::Point v1_centerpoint = v1_bounds.CenterPoint();
  gfx::Point v2_centerpoint = v2_bounds.CenterPoint();
  gfx::Point v1_origin = v1_bounds.origin();
  gfx::Point v2_origin = v2_bounds.origin();
  gfx::Rect r1(10, 10, 110, 15);
  gfx::Rect r2(106, 1, 98, 98);
  gfx::Rect r3(0, 0, 300, 300);
  gfx::Rect r4(115, 342, 200, 10);

  // Test calls into View::HitTestPoint().
  EXPECT_TRUE(
      v1->HitTestPoint(ConvertPointFromWidgetToView(v1, v1_centerpoint)));
  EXPECT_TRUE(
      v2->HitTestPoint(ConvertPointFromWidgetToView(v2, v2_centerpoint)));

  EXPECT_TRUE(v1->HitTestPoint(ConvertPointFromWidgetToView(v1, v1_origin)));
  EXPECT_FALSE(v2->HitTestPoint(ConvertPointFromWidgetToView(v2, v2_origin)));

  // Test calls into View::HitTestRect().
  EXPECT_TRUE(v1->HitTestRect(ConvertRectFromWidgetToView(v1, r1)));
  EXPECT_FALSE(v2->HitTestRect(ConvertRectFromWidgetToView(v2, r1)));

  EXPECT_FALSE(v1->HitTestRect(ConvertRectFromWidgetToView(v1, r2)));
  EXPECT_TRUE(v2->HitTestRect(ConvertRectFromWidgetToView(v2, r2)));

  EXPECT_TRUE(v1->HitTestRect(ConvertRectFromWidgetToView(v1, r3)));
  EXPECT_TRUE(v2->HitTestRect(ConvertRectFromWidgetToView(v2, r3)));

  EXPECT_FALSE(v1->HitTestRect(ConvertRectFromWidgetToView(v1, r4)));
  EXPECT_FALSE(v2->HitTestRect(ConvertRectFromWidgetToView(v2, r4)));

  // Test calls into View::GetEventHandlerForPoint().
  EXPECT_EQ(v1, root_view->GetEventHandlerForPoint(v1_centerpoint));
  EXPECT_EQ(v2, root_view->GetEventHandlerForPoint(v2_centerpoint));

  EXPECT_EQ(v1, root_view->GetEventHandlerForPoint(v1_origin));
  EXPECT_EQ(root_view, root_view->GetEventHandlerForPoint(v2_origin));

  // Test calls into View::GetTooltipHandlerForPoint().
  EXPECT_EQ(v1, root_view->GetTooltipHandlerForPoint(v1_centerpoint));
  EXPECT_EQ(v2, root_view->GetTooltipHandlerForPoint(v2_centerpoint));

  EXPECT_EQ(v1, root_view->GetTooltipHandlerForPoint(v1_origin));
  EXPECT_EQ(root_view, root_view->GetTooltipHandlerForPoint(v2_origin));

  EXPECT_FALSE(v1->GetTooltipHandlerForPoint(v2_origin));
}

TEST_F(ViewTargeterTest, FavorChildContainingHitBounds) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget->Init(std::move(init_params));

  View* content = widget->SetContentsView(std::make_unique<View>());
  content->SetBounds(0, 0, 50, 50);
  View* child = content->AddChildView(std::make_unique<View>());
  child->SetBounds(2, 2, 50, 50);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  ui::EventTargeter* targeter = root_view->targeter();

  gfx::RectF bounding_box(gfx::PointF(4.f, 4.f), gfx::SizeF(42.f, 42.f));
  ui::GestureEventDetails details(ui::EventType::kGestureTap);
  details.set_bounding_box(bounding_box);
  ui::GestureEvent tap = CreateTestGestureEvent(details);

  EXPECT_EQ(child, targeter->FindTargetForEvent(root_view, &tap));
}

}  // namespace test
}  // namespace views
