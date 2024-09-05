// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/element_tracker_views.h"

#include <iterator>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementID);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementID2);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType);

namespace {

enum ElementEventType { kShown, kActivated, kHidden, kCustom };

View* ElementToView(ui::TrackedElement* element) {
  auto* const view_element = element->AsA<TrackedElementViews>();
  return view_element ? view_element->view() : nullptr;
}

// A subclass of View that has metadata.
class TypedView : public View {
  METADATA_HEADER(TypedView, View)
};

BEGIN_METADATA(TypedView)
END_METADATA

// Watches events on the ElementTracker and converts the resulting values back
// into Views from the original ui::TrackedElement objects. Monitoring
// callbacks in this way could be done with gmock but the boilerplate would be
// unfortunately complicated (for some events, the correct parameters are not
// known until after the call is made, since the call itself might create the
// element in question). So instead we use this helper class.
class ElementEventWatcher {
 public:
  // Watches the specified `event_type` on Views with identifier `id` in
  // `context`.
  ElementEventWatcher(ui::ElementIdentifier id,
                      ui::ElementContext context,
                      ElementEventType event_type)
      : id_(id), event_type_(event_type) {
    auto callback = base::BindRepeating(&ElementEventWatcher::OnEvent,
                                        base::Unretained(this));
    ui::ElementTracker* const tracker = ui::ElementTracker::GetElementTracker();
    switch (event_type_) {
      case ElementEventType::kShown:
        subscription_ = tracker->AddElementShownCallback(id, context, callback);
        break;
      case ElementEventType::kActivated:
        subscription_ =
            tracker->AddElementActivatedCallback(id, context, callback);
        break;
      case ElementEventType::kHidden:
        subscription_ =
            tracker->AddElementHiddenCallback(id, context, callback);
        break;
      case ElementEventType::kCustom:
        subscription_ = tracker->AddCustomEventCallback(id, context, callback);
        break;
    }
  }

  int event_count() const { return event_count_; }
  View* last_view() { return last_view_; }

 private:
  void OnEvent(ui::TrackedElement* element) {
    if (event_type_ != ElementEventType::kCustom)
      EXPECT_EQ(id_, element->identifier());
    last_view_ = ElementToView(element);
    ++event_count_;
  }

  const ui::ElementIdentifier id_;
  const ElementEventType event_type_;
  ui::ElementTracker::Subscription subscription_;
  int event_count_ = 0;
  raw_ptr<View, DanglingUntriaged> last_view_ = nullptr;
};

ElementTrackerViews::ViewList ElementsToViews(
    ui::ElementTracker::ElementList elements) {
  ElementTrackerViews::ViewList result;
  base::ranges::transform(elements, std::back_inserter(result),
                          [](ui::TrackedElement* element) {
                            return element->AsA<TrackedElementViews>()->view();
                          });
  return result;
}

}  // namespace

class ElementTrackerViewsTest : public ViewsTestBase {
 public:
  ElementTrackerViewsTest() = default;
  ~ElementTrackerViewsTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateWidget();
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  ui::ElementContext context() const {
    return ui::ElementContext(widget_.get());
  }

  std::unique_ptr<Widget> CreateWidget() {
    auto widget = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget->Init(std::move(params));
    return widget;
  }

 protected:
  std::unique_ptr<Widget> widget_;
};

TEST_F(ElementTrackerViewsTest, ViewShownByAddingToWidgetSendsNotification) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kShown);
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(0, watcher.event_count());
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest,
       ViewHiddenByRemovingFromWidgetSendsNotification) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kHidden);
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(0, watcher.event_count());
  auto* const view = widget_->SetContentsView(std::make_unique<View>());
  auto* const button = view->AddChildView(std::move(button_ptr));
  EXPECT_EQ(0, watcher.event_count());
  button_ptr = view->RemoveChildViewT(button);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, ViewShownAfterAddingToWidgetSendsNotification) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kShown);
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetVisible(false);
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(0, watcher.event_count());
  button->SetVisible(true);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest,
       ViewHiddenAfterAddingToWidgetSendsNotification) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kHidden);
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(0, watcher.event_count());
  button->SetVisible(false);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, SettingIDOnVisibleViewSendsNotification) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kShown);
  auto button_ptr = std::make_unique<LabelButton>();
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(0, watcher.event_count());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, ClearingIDOnVisibleViewSendsNotification) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kHidden);
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(0, watcher.event_count());
  button->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, ChangingIDOnVisibleViewSendsNotification) {
  ElementEventWatcher shown(kTestElementID, context(),
                            ElementEventType::kShown);
  ElementEventWatcher hidden(kTestElementID, context(),
                             ElementEventType::kHidden);
  ElementEventWatcher shown2(kTestElementID2, context(),
                             ElementEventType::kShown);
  ElementEventWatcher hidden2(kTestElementID2, context(),
                              ElementEventType::kHidden);
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(0, hidden.event_count());
  EXPECT_EQ(0, shown2.event_count());
  EXPECT_EQ(0, hidden2.event_count());
  button->SetProperty(kElementIdentifierKey, kTestElementID2);
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(1, hidden.event_count());
  EXPECT_EQ(1, shown2.event_count());
  EXPECT_EQ(0, hidden2.event_count());
  button->SetVisible(false);
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(1, hidden.event_count());
  EXPECT_EQ(1, shown2.event_count());
  EXPECT_EQ(1, hidden2.event_count());
}

TEST_F(ElementTrackerViewsTest, ButtonPressedSendsActivatedSignal) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kActivated);
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(0, watcher.event_count());
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(0, watcher.event_count());

  // Test mouse click.
  constexpr gfx::Point kPressPoint(10, 10);
  button->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, kPressPoint, kPressPoint,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  button->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMousePressed, kPressPoint, kPressPoint,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());

  // Test accessible keypress.
  views::test::InteractionTestUtilSimulatorViews::PressButton(button);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, MenuButtonPressedSendsActivatedSignal) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kActivated);
  size_t pressed_count = 0;
  auto button_ptr = std::make_unique<MenuButton>(
      base::BindLambdaForTesting([&](const ui::Event&) { ++pressed_count; }));
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(0, watcher.event_count());
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(0, watcher.event_count());

  // Test mouse click.
  constexpr gfx::Point kPressPoint(10, 10);
  button->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, kPressPoint, kPressPoint,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(1U, pressed_count);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());

  // Test accessible keypress.
  views::test::InteractionTestUtilSimulatorViews::PressButton(button);
  EXPECT_EQ(2U, pressed_count);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, SendCustomEventWithNamedElement) {
  ElementEventWatcher watcher(kCustomEventType, context(),
                              ElementEventType::kCustom);
  auto* const target = widget_->SetContentsView(std::make_unique<View>());
  target->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(0, watcher.event_count());
  ElementTrackerViews::GetInstance()->NotifyCustomEvent(kCustomEventType,
                                                        target);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(target, watcher.last_view());
  // Send an event with a different ID (which happens to be the element's ID;
  // this shouldn't happen but we should handle it gracefully).
  ElementTrackerViews::GetInstance()->NotifyCustomEvent(kTestElementID, target);
  EXPECT_EQ(1, watcher.event_count());
  // Send another event.
  ElementTrackerViews::GetInstance()->NotifyCustomEvent(kCustomEventType,
                                                        target);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(target, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, SendCustomEventWithUnnamedElement) {
  ElementEventWatcher watcher(kCustomEventType, context(),
                              ElementEventType::kCustom);
  auto* const target = widget_->SetContentsView(std::make_unique<View>());
  // View has no pre-set identifier, but this should still work.
  EXPECT_EQ(0, watcher.event_count());
  ElementTrackerViews::GetInstance()->NotifyCustomEvent(kCustomEventType,
                                                        target);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(target, watcher.last_view());
  // Send an extraneous event.
  ElementTrackerViews::GetInstance()->NotifyCustomEvent(kTestElementID, target);
  EXPECT_EQ(1, watcher.event_count());
  // Send another event.
  ElementTrackerViews::GetInstance()->NotifyCustomEvent(kCustomEventType,
                                                        target);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(target, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, HandlesCreateWithTheSameIDMultipleTimes) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kShown);

  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
  button_ptr = widget_->GetRootView()->RemoveChildViewT(button);

  auto button_ptr2 = std::make_unique<LabelButton>();
  button_ptr2->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const button2 = widget_->SetContentsView(std::move(button_ptr2));
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(button2, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, HandlesReshowingTheSameView) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kShown);

  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());

  button->SetVisible(false);
  EXPECT_EQ(1, watcher.event_count());

  button->SetVisible(true);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
}

TEST_F(ElementTrackerViewsTest, CanLookupViewByIdentifier) {
  // Should initially be null.
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));

  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);

  // Because the button is not attached to a widget, it will not be returned for
  // the current context (which is associated with a widget).
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));

  // Adding the view to a widget will cause it to be returned in the current
  // context.
  auto* button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(
      button,
      ElementToView(ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kTestElementID, context())));

  // Once the view is destroyed, however, the result should be null again.
  widget_->GetRootView()->RemoveChildViewT(button);
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));

  // Create a second view with the same ID and verify that the new pointer is
  // returned.
  button = widget_->SetContentsView(std::make_unique<LabelButton>());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(
      button,
      ElementToView(ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kTestElementID, context())));

  // When the view is deleted, the result once again becomes null.
  widget_->GetRootView()->RemoveChildViewT(button);
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, CanLookUpElementByIdentifier) {
  // Should initially be null.
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));

  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);

  // Because the button is not attached to a widget, it will not be returned for
  // the current context (which is associated with a widget).
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));

  // Adding the view to a widget will cause it to be returned in the current
  // context.
  auto* button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_EQ(button, ui::ElementTracker::GetElementTracker()
                        ->GetUniqueElement(kTestElementID, context())
                        ->AsA<TrackedElementViews>()
                        ->view());

  // Hiding the view will make the view not findable through the base element
  // tracker.
  button->SetVisible(false);
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));

  // Showing the view will bring it back.
  button->SetVisible(true);
  EXPECT_EQ(button, ui::ElementTracker::GetElementTracker()
                        ->GetUniqueElement(kTestElementID, context())
                        ->AsA<TrackedElementViews>()
                        ->view());

  // Once the view is destroyed, however, the result should be null again.
  widget_->GetRootView()->RemoveChildViewT(button);
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));

  // Create a second view with the same ID and verify that the new pointer is
  // returned.
  button = widget_->SetContentsView(std::make_unique<LabelButton>());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(button, ui::ElementTracker::GetElementTracker()
                        ->GetUniqueElement(kTestElementID, context())
                        ->AsA<TrackedElementViews>()
                        ->view());

  // When the view is deleted, the result once again becomes null.
  widget_->GetRootView()->RemoveChildViewT(button);
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, CanGetFirstViewByIdentifier) {
  // Should initially be null.
  EXPECT_EQ(nullptr,
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kTestElementID, context()));

  // Add two buttons with the same identifier.
  auto* contents = widget_->SetContentsView(std::make_unique<View>());
  auto* button = contents->AddChildView(std::make_unique<LabelButton>());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* button2 = contents->AddChildView(std::make_unique<LabelButton>());
  button2->SetProperty(kElementIdentifierKey, kTestElementID);

  // The first button should be returned.
  EXPECT_EQ(
      button,
      ElementToView(
          ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
              kTestElementID, context())));

  // Remove the first button. The second should now be returned.
  contents->RemoveChildViewT(button);
  EXPECT_EQ(
      button2,
      ElementToView(
          ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
              kTestElementID, context())));

  // Remove the second button. There will be no matching views.
  contents->RemoveChildViewT(button2);
  EXPECT_EQ(nullptr,
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, CanGetFirstElementByIdentifier) {
  // Should initially be null.
  EXPECT_EQ(nullptr,
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kTestElementID, context()));

  // Add two buttons with the same identifier.
  auto* contents = widget_->SetContentsView(std::make_unique<View>());
  auto* button = contents->AddChildView(std::make_unique<LabelButton>());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* button2 = contents->AddChildView(std::make_unique<LabelButton>());
  button2->SetProperty(kElementIdentifierKey, kTestElementID);

  // The first button should be returned.
  EXPECT_EQ(button, ui::ElementTracker::GetElementTracker()
                        ->GetFirstMatchingElement(kTestElementID, context())
                        ->AsA<TrackedElementViews>()
                        ->view());

  // Set the buttons' visibility; this should change whether the element tracker
  // sees them.
  button->SetVisible(false);
  EXPECT_EQ(button2, ui::ElementTracker::GetElementTracker()
                         ->GetFirstMatchingElement(kTestElementID, context())
                         ->AsA<TrackedElementViews>()
                         ->view());
  button2->SetVisible(false);
  EXPECT_EQ(nullptr,
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kTestElementID, context()));

  // The second button is now the first to become visible in the base tracker.
  button2->SetVisible(true);
  button->SetVisible(true);
  EXPECT_EQ(button2, ui::ElementTracker::GetElementTracker()
                         ->GetFirstMatchingElement(kTestElementID, context())
                         ->AsA<TrackedElementViews>()
                         ->view());

  // Remove the second button. The first should now be returned.
  contents->RemoveChildViewT(button2);
  EXPECT_EQ(button, ui::ElementTracker::GetElementTracker()
                        ->GetFirstMatchingElement(kTestElementID, context())
                        ->AsA<TrackedElementViews>()
                        ->view());

  // Remove the first button. There will be no matching views.
  contents->RemoveChildViewT(button);
  EXPECT_EQ(nullptr,
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, CanGetAllViewsByIdentifier) {
  // Should initially be empty.
  ElementTrackerViews::ViewList expected;
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));

  // Add two buttons with the same identifier.
  auto* contents = widget_->SetContentsView(std::make_unique<View>());
  auto* button = contents->AddChildView(std::make_unique<LabelButton>());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* button2 = contents->AddChildView(std::make_unique<LabelButton>());
  button2->SetProperty(kElementIdentifierKey, kTestElementID);

  // All buttons should be returned.
  expected = ElementTrackerViews::ViewList{button, button2};
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));

  // Remove the first button. The second should now be returned.
  contents->RemoveChildViewT(button);
  expected = ElementTrackerViews::ViewList{button2};
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));

  // Remove the second button. There will be no matching views.
  contents->RemoveChildViewT(button2);
  expected.clear();
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));
}

TEST_F(ElementTrackerViewsTest, CanGetAllElementsByIdentifier) {
  // Should initially be empty.
  ElementTrackerViews::ViewList expected;
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));

  // Add two buttons with the same identifier.
  auto* contents = widget_->SetContentsView(std::make_unique<View>());
  auto* button = contents->AddChildView(std::make_unique<LabelButton>());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* button2 = contents->AddChildView(std::make_unique<LabelButton>());
  button2->SetProperty(kElementIdentifierKey, kTestElementID);

  // Both buttons should be returned.
  expected = ElementTrackerViews::ViewList{button, button2};
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));

  // Set the buttons' visibility; this should change whether the element tracker
  // sees them.
  button->SetVisible(false);
  expected = ElementTrackerViews::ViewList{button2};
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));
  button2->SetVisible(false);
  expected.clear();
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));

  // The second button is now the first to become visible in the base tracker.
  button2->SetVisible(true);
  button->SetVisible(true);
  expected = ElementTrackerViews::ViewList{button2, button};
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));

  // Remove the second button. Only the first should now be returned.
  contents->RemoveChildViewT(button2);
  expected = ElementTrackerViews::ViewList{button};
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));

  // Remove the first button. There will be no matching views.
  contents->RemoveChildViewT(button);
  expected.clear();
  EXPECT_EQ(expected,
            ElementsToViews(
                ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
                    kTestElementID, context())));
}

TEST_F(ElementTrackerViewsTest, CanGetVisibilityByIdentifier) {
  // Should initially be false.
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));

  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);

  // Because the button is not attached to a widget, it will not be counted as
  // visible.
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));

  // Adding the view to a widget will cause it to be counted as visible.
  auto* button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));

  // Once the view is destroyed, however, the result should be false again.
  widget_->GetRootView()->RemoveChildViewT(button);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));

  // Create a second view with the same ID but start it as not visible.
  button = widget_->SetContentsView(std::make_unique<LabelButton>());
  button->SetVisible(false);
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));

  // Now set the visibility to true.
  button->SetVisible(true);
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));

  // Set visibility to false again.
  button->SetVisible(false);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, CanLookupElementByView) {
  // Should initially be false.
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));

  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);

  // The button is not attached to a widget so there is no associated element
  // object.
  EXPECT_EQ(nullptr, ElementTrackerViews::GetInstance()->GetElementForView(
                         button_ptr.get()));

  // Adding the (visible) view to a widget will cause an element to be
  // generated.
  auto* button = widget_->SetContentsView(std::move(button_ptr));
  EXPECT_NE(nullptr,
            ElementTrackerViews::GetInstance()->GetElementForView(button));

  // Once the view is destroyed, however, the result should be false again.
  widget_->GetRootView()->RemoveChildView(button);
  EXPECT_EQ(nullptr,
            ElementTrackerViews::GetInstance()->GetElementForView(button));
  delete button;

  // Create a second view with the same ID but start it as not visible.
  button = widget_->SetContentsView(std::make_unique<LabelButton>());
  button->SetVisible(false);
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(nullptr,
            ElementTrackerViews::GetInstance()->GetElementForView(button));

  // Now set the visibility to true.
  button->SetVisible(true);
  EXPECT_NE(nullptr,
            ElementTrackerViews::GetInstance()->GetElementForView(button));

  // Set visibility to false again.
  button->SetVisible(false);
  EXPECT_EQ(nullptr,
            ElementTrackerViews::GetInstance()->GetElementForView(button));
}

TEST_F(ElementTrackerViewsTest, AssignTemporaryId) {
  auto* button = widget_->SetContentsView(std::make_unique<LabelButton>());
  DCHECK(!button->GetProperty(kElementIdentifierKey));

  TrackedElementViews* element =
      ElementTrackerViews::GetInstance()->GetElementForView(button);
  EXPECT_EQ(nullptr, element);
  element = ElementTrackerViews::GetInstance()->GetElementForView(button, true);
  EXPECT_NE(nullptr, element);
  EXPECT_EQ(ui::ElementTracker::kTemporaryIdentifier,
            button->GetProperty(kElementIdentifierKey));
  EXPECT_EQ(element, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         ui::ElementTracker::kTemporaryIdentifier, context()));
}

// The following tests ensure conformity with the different platforms' Views
// implementation to ensure that Views are reported as visible to the user at
// the correct times, including during Widget close/delete.

TEST_F(ElementTrackerViewsTest, ParentNotVisibleWhenAddedToWidget) {
  View* const contents = widget_->SetContentsView(std::make_unique<View>());
  contents->SetVisible(false);
  auto child_ptr = std::make_unique<View>();
  child_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  contents->AddChildView(std::move(child_ptr));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  contents->SetVisible(true);
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, WidgetNotVisibleWhenAddedToWidget) {
  View* const contents = widget_->SetContentsView(std::make_unique<View>());
  widget_->Hide();
  auto child_ptr = std::make_unique<View>();
  child_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  contents->AddChildView(std::move(child_ptr));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  widget_->Show();
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, ParentHidden) {
  View* const contents = widget_->SetContentsView(std::make_unique<View>());
  auto child_ptr = std::make_unique<View>();
  child_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  contents->AddChildView(std::move(child_ptr));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  contents->SetVisible(false);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, WidgetHidden) {
  View* const contents = widget_->SetContentsView(std::make_unique<View>());
  auto child_ptr = std::make_unique<View>();
  child_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  contents->AddChildView(std::move(child_ptr));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  widget_->Hide();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, WidgetClosed) {
  View* const contents = widget_->SetContentsView(std::make_unique<View>());
  auto child_ptr = std::make_unique<View>();
  child_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  contents->AddChildView(std::move(child_ptr));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  widget_->Close();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, WidgetDestroyed) {
  View* const contents = widget_->SetContentsView(std::make_unique<View>());
  auto child_ptr = std::make_unique<View>();
  child_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  contents->AddChildView(std::move(child_ptr));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
  widget_.reset();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context()));
}

TEST_F(ElementTrackerViewsTest, WidgetShownAfterAdd) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);
  auto child_ptr = std::make_unique<View>();
  child_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context));
  contents->AddChildView(std::move(child_ptr));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context));
  widget->Show();
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementID, context));
}

// This is a gross corner case where a Widget might not report IsVisible()
// during show, but we're still showing views and could conceivably add another
// view as part of a callback.
TEST_F(ElementTrackerViewsTest, AddedDuringWidgetShow) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  View* const child1 = contents->AddChildView(std::make_unique<View>());
  View* const child2 = contents->AddChildView(std::make_unique<View>());
  child1->SetProperty(kElementIdentifierKey, kTestElementID);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kTestElementID,
          ElementTrackerViews::GetContextForWidget(widget.get()),
          base::BindLambdaForTesting([&](ui::TrackedElement*) {
            child2->SetProperty(kElementIdentifierKey, kTestElementID2);
          }));

  bool called = false;

  auto subscription2 =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kTestElementID2,
          ElementTrackerViews::GetContextForWidget(widget.get()),
          base::BindLambdaForTesting([&](ui::TrackedElement* element) {
            EXPECT_EQ(child2, element->AsA<TrackedElementViews>()->view());
            called = true;
          }));

  test::WidgetVisibleWaiter visible_waiter(widget.get());
  widget->Show();
  visible_waiter.Wait();
  EXPECT_TRUE(called);

  // Now verify that hiding a widget which we engaged during initial Show(),
  // without destroying the views, causes the elements to be hidden.
  subscription2 =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          kTestElementID2,
          ElementTrackerViews::GetContextForWidget(widget.get()),
          base::BindLambdaForTesting([&](ui::TrackedElement* element) {
            EXPECT_EQ(child2, element->AsA<TrackedElementViews>()->view());
            called = true;
          }));

  called = false;
  widget->Hide();
  EXPECT_TRUE(called);
}

TEST_F(ElementTrackerViewsTest, CleansUpWidgetTrackers) {
  auto widget1 = CreateWidget();
  View* const contents1 = widget1->SetContentsView(std::make_unique<View>());
  contents1->SetProperty(kElementIdentifierKey, kTestElementID);
  auto widget2 = CreateWidget();
  View* const contents2 = widget1->SetContentsView(std::make_unique<View>());
  contents2->SetProperty(kElementIdentifierKey, kTestElementID);

  test::WidgetVisibleWaiter waiter1(widget1.get());
  test::WidgetVisibleWaiter waiter2(widget2.get());
  widget1->Show();
  widget2->Show();
  waiter1.Wait();
  waiter2.Wait();

  widget1->Hide();
  test::WidgetDestroyedWaiter destroyed_waiter(widget2.get());
  widget2->Close();
  destroyed_waiter.Wait();

  EXPECT_TRUE(ElementTrackerViews::GetInstance()->widget_trackers_.empty());
}

TEST_F(ElementTrackerViewsTest, GetUniqueView) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);
  EXPECT_EQ(nullptr, ElementTrackerViews::GetInstance()->GetUniqueView(
                         kTestElementID, context));

  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(contents, ElementTrackerViews::GetInstance()->GetUniqueView(
                          kTestElementID, context));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(nullptr, ElementTrackerViews::GetInstance()->GetUniqueView(
                         kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetUniqueViewAs) {
  auto widget = CreateWidget();
  TypedView* const contents =
      widget->SetContentsView(std::make_unique<TypedView>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);
  EXPECT_EQ(nullptr,
            ElementTrackerViews::GetInstance()->GetUniqueViewAs<TypedView>(
                kTestElementID, context));

  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(contents,
            ElementTrackerViews::GetInstance()->GetUniqueViewAs<TypedView>(
                kTestElementID, context));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(nullptr,
            ElementTrackerViews::GetInstance()->GetUniqueViewAs<TypedView>(
                kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetFirstMatchingViewWithSingleView) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);
  EXPECT_EQ(nullptr, ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                         kTestElementID, context));

  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(contents, ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                          kTestElementID, context));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(nullptr, ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                         kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetFirstMatchingViewAs) {
  auto widget = CreateWidget();
  TypedView* const contents =
      widget->SetContentsView(std::make_unique<TypedView>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);
  EXPECT_EQ(
      nullptr,
      ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<TypedView>(
          kTestElementID, context));

  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(
      contents,
      ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<TypedView>(
          kTestElementID, context));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(
      nullptr,
      ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<TypedView>(
          kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetFirstMatchingViewWithMultipleViews) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  View* const v1 = contents->AddChildView(std::make_unique<View>());
  View* const v2 = contents->AddChildView(std::make_unique<View>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);

  v1->SetProperty(kElementIdentifierKey, kTestElementID);
  v2->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(v1, ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                    kTestElementID, context));

  v1->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(v2, ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                    kTestElementID, context));

  v2->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(nullptr, ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                         kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetFirstMatchingViewWithNonViewsElements) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);

  ui::test::TestElementPtr test_element1 =
      std::make_unique<ui::test::TestElement>(kTestElementID, context);
  ui::test::TestElementPtr test_element2 =
      std::make_unique<ui::test::TestElement>(kTestElementID, context);

  test_element1->Show();
  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  test_element2->Show();
  EXPECT_EQ(contents, ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                          kTestElementID, context));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(nullptr, ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                         kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetAllMatchingViewsWithSingleView) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);
  EXPECT_EQ(ElementTrackerViews::ViewList(),
            ElementTrackerViews::GetInstance()->GetAllMatchingViews(
                kTestElementID, context));

  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  const ElementTrackerViews::ViewList expected = {contents};
  EXPECT_EQ(expected, ElementTrackerViews::GetInstance()->GetAllMatchingViews(
                          kTestElementID, context));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(ElementTrackerViews::ViewList(),
            ElementTrackerViews::GetInstance()->GetAllMatchingViews(
                kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetAllMatchingViewsWithMultipleViews) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  View* const v1 = contents->AddChildView(std::make_unique<View>());
  View* const v2 = contents->AddChildView(std::make_unique<View>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);

  v1->SetProperty(kElementIdentifierKey, kTestElementID);
  v2->SetProperty(kElementIdentifierKey, kTestElementID);
  ElementTrackerViews::ViewList expected = {v1, v2};
  EXPECT_EQ(expected, ElementTrackerViews::GetInstance()->GetAllMatchingViews(
                          kTestElementID, context));

  v1->ClearProperty(kElementIdentifierKey);
  expected = {v2};
  EXPECT_EQ(expected, ElementTrackerViews::GetInstance()->GetAllMatchingViews(
                          kTestElementID, context));

  v2->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(ElementTrackerViews::ViewList(),
            ElementTrackerViews::GetInstance()->GetAllMatchingViews(
                kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetAllMatchingViewsWithNonViewsElements) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);

  ui::test::TestElementPtr test_element1 =
      std::make_unique<ui::test::TestElement>(kTestElementID, context);
  ui::test::TestElementPtr test_element2 =
      std::make_unique<ui::test::TestElement>(kTestElementID, context);

  test_element1->Show();
  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  test_element2->Show();
  const ElementTrackerViews::ViewList expected = {contents};
  EXPECT_EQ(expected, ElementTrackerViews::GetInstance()->GetAllMatchingViews(
                          kTestElementID, context));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_EQ(ElementTrackerViews::ViewList(),
            ElementTrackerViews::GetInstance()->GetAllMatchingViews(
                kTestElementID, context));
}

TEST_F(ElementTrackerViewsTest, GetAllViewsInAnyContextWithSingleView) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  widget->Show();
  EXPECT_THAT(
      ElementTrackerViews::GetInstance()->GetAllMatchingViewsInAnyContext(
          kTestElementID),
      testing::IsEmpty());

  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_THAT(
      ElementTrackerViews::GetInstance()->GetAllMatchingViewsInAnyContext(
          kTestElementID),
      testing::UnorderedElementsAre(contents));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_THAT(
      ElementTrackerViews::GetInstance()->GetAllMatchingViewsInAnyContext(
          kTestElementID),
      testing::IsEmpty());
}

TEST_F(ElementTrackerViewsTest, GetAllViewsInAnyContextWithMultipleViews) {
  auto widget = CreateWidget();
  auto widget2 = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  View* const v1 = contents->AddChildView(std::make_unique<View>());
  View* const v2 = contents->AddChildView(std::make_unique<View>());
  View* const contents2 = widget2->SetContentsView(std::make_unique<View>());
  View* const v3 = contents2->AddChildView(std::make_unique<View>());
  View* const v4 = contents2->AddChildView(std::make_unique<View>());
  widget->Show();
  widget2->Show();

  v1->SetProperty(kElementIdentifierKey, kTestElementID);
  v2->SetProperty(kElementIdentifierKey, kTestElementID);
  v3->SetProperty(kElementIdentifierKey, kTestElementID);
  v4->SetProperty(kElementIdentifierKey, kTestElementID2);
  EXPECT_THAT(
      ElementTrackerViews::GetInstance()->GetAllMatchingViewsInAnyContext(
          kTestElementID),
      testing::UnorderedElementsAre(v1, v2, v3));

  v1->ClearProperty(kElementIdentifierKey);
  EXPECT_THAT(
      ElementTrackerViews::GetInstance()->GetAllMatchingViewsInAnyContext(
          kTestElementID),
      testing::UnorderedElementsAre(v2, v3));

  v2->ClearProperty(kElementIdentifierKey);
  EXPECT_THAT(
      ElementTrackerViews::GetInstance()->GetAllMatchingViewsInAnyContext(
          kTestElementID),
      testing::UnorderedElementsAre(v3));
}

TEST_F(ElementTrackerViewsTest, GetAllViewsInAnyContextWithNonViewsElements) {
  auto widget = CreateWidget();
  View* const contents = widget->SetContentsView(std::make_unique<View>());
  widget->Show();
  const ui::ElementContext context =
      ElementTrackerViews::GetContextForView(contents);

  ui::test::TestElementPtr test_element1 =
      std::make_unique<ui::test::TestElement>(kTestElementID, context);
  ui::test::TestElementPtr test_element2 =
      std::make_unique<ui::test::TestElement>(kTestElementID, context);

  test_element1->Show();
  contents->SetProperty(kElementIdentifierKey, kTestElementID);
  test_element2->Show();
  EXPECT_THAT(
      ElementTrackerViews::GetInstance()->GetAllMatchingViewsInAnyContext(
          kTestElementID),
      testing::UnorderedElementsAre(contents));

  contents->ClearProperty(kElementIdentifierKey);
  EXPECT_THAT(
      ElementTrackerViews::GetInstance()->GetAllMatchingViewsInAnyContext(
          kTestElementID),
      testing::IsEmpty());
}

// Verifies that Views on different Widgets are differentiated by the system.
class ElementTrackerTwoWidgetTest : public ElementTrackerViewsTest {
 public:
  ElementTrackerTwoWidgetTest() = default;
  ~ElementTrackerTwoWidgetTest() override = default;

  void SetUp() override {
    ElementTrackerViewsTest::SetUp();

    widget2_ = CreateWidget();
    widget2_->Show();
  }

  void TearDown() override {
    widget2_.reset();
    ElementTrackerViewsTest::TearDown();
  }

  ui::ElementContext context2() const {
    return ui::ElementContext(widget2_.get());
  }

 protected:
  std::unique_ptr<Widget> widget2_;
};

TEST_F(ElementTrackerTwoWidgetTest, ViewMovedToDifferentWidgetGeneratesEvents) {
  ElementEventWatcher shown(kTestElementID, context(),
                            ElementEventType::kShown);
  ElementEventWatcher hidden(kTestElementID, context(),
                             ElementEventType::kHidden);
  ElementEventWatcher shown2(kTestElementID, context2(),
                             ElementEventType::kShown);
  ElementEventWatcher hidden2(kTestElementID, context2(),
                              ElementEventType::kHidden);
  auto* const view = widget_->SetContentsView(std::make_unique<View>());
  auto* const view2 = widget2_->SetContentsView(std::make_unique<View>());
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  // Add to first widget.
  auto* const button = view->AddChildView(std::move(button_ptr));
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(0, hidden.event_count());
  EXPECT_EQ(0, shown2.event_count());
  EXPECT_EQ(0, hidden2.event_count());
  // Move to second widget.
  view2->AddChildView(button);
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(1, hidden.event_count());
  EXPECT_EQ(1, shown2.event_count());
  EXPECT_EQ(0, hidden2.event_count());
  // Destroy the second widget.
  widget2_.reset();
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(1, hidden.event_count());
  EXPECT_EQ(1, shown2.event_count());
  EXPECT_EQ(1, hidden2.event_count());
}

TEST_F(ElementTrackerTwoWidgetTest, CanLookUpViewsOnMultipleWidgets) {
  auto* button = widget_->SetContentsView(std::make_unique<LabelButton>());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* button2 = widget2_->SetContentsView(std::make_unique<LabelButton>());
  button2->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(
      button,
      ElementToView(ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kTestElementID, context())));
  EXPECT_EQ(
      button2,
      ElementToView(ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kTestElementID, context2())));
  widget_->GetRootView()->RemoveChildViewT(button);
  widget2_->GetRootView()->RemoveChildViewT(button2);
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context()));
  EXPECT_EQ(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kTestElementID, context2()));
}

TEST_F(ElementTrackerTwoWidgetTest,
       MakingViewsVisibleSendsNotificationsToCorrectListeners) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kShown);
  ElementEventWatcher watcher2(kTestElementID, context2(),
                               ElementEventType::kShown);
  auto* const button =
      widget_->SetContentsView(std::make_unique<LabelButton>());
  auto* const button2 =
      widget2_->SetContentsView(std::make_unique<LabelButton>());
  EXPECT_EQ(0, watcher.event_count());
  EXPECT_EQ(0, watcher2.event_count());

  // Each listener should be notified when the appropriate button is shown.
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
  EXPECT_EQ(0, watcher2.event_count());
  button2->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
  EXPECT_EQ(1, watcher2.event_count());
  EXPECT_EQ(button2, watcher2.last_view());

  // Each listener should be notified when the appropriate button is shown.
  button->SetVisible(false);
  button->SetVisible(true);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(1, watcher2.event_count());

  // Hide and show several times to verify events are still set.
  button->SetVisible(false);
  button2->SetVisible(false);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(1, watcher2.event_count());
  button2->SetVisible(true);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(2, watcher2.event_count());
  button->SetVisible(true);
  EXPECT_EQ(3, watcher.event_count());
  EXPECT_EQ(2, watcher2.event_count());
}

TEST_F(ElementTrackerTwoWidgetTest,
       ButtonPressedSendsNotificationsToCorrectListeners) {
  ElementEventWatcher watcher(kTestElementID, context(),
                              ElementEventType::kActivated);
  ElementEventWatcher watcher2(kTestElementID, context2(),
                               ElementEventType::kActivated);
  auto* const button =
      widget_->SetContentsView(std::make_unique<LabelButton>());
  auto* const button2 =
      widget2_->SetContentsView(std::make_unique<LabelButton>());
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  button2->SetProperty(kElementIdentifierKey, kTestElementID);
  EXPECT_EQ(0, watcher.event_count());
  EXPECT_EQ(0, watcher2.event_count());

  // Test mouse click.
  constexpr gfx::Point kPressPoint(10, 10);
  button->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, kPressPoint, kPressPoint,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  button->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMousePressed, kPressPoint, kPressPoint,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
  EXPECT_EQ(0, watcher2.event_count());

  // Click other button.
  button2->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, kPressPoint, kPressPoint,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  button2->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMousePressed, kPressPoint, kPressPoint,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(button, watcher.last_view());
  EXPECT_EQ(1, watcher2.event_count());
  EXPECT_EQ(button2, watcher2.last_view());

  // Test accessible keypress.
  views::test::InteractionTestUtilSimulatorViews::PressButton(button2);
  EXPECT_EQ(1, watcher.event_count());
  EXPECT_EQ(2, watcher2.event_count());
  views::test::InteractionTestUtilSimulatorViews::PressButton(button);
  EXPECT_EQ(2, watcher.event_count());
  EXPECT_EQ(2, watcher2.event_count());
}

// The following are variations on the "view's context changes when it moves
// between widgets" test, but with an override context callback modifying
// what context is returned for one or both widgets.

TEST_F(ElementTrackerTwoWidgetTest, OverrideContextCallbackCollapsesContexts) {
  const ui::ElementContext kContext{1};
  ElementTrackerViews::SetContextOverrideCallback(
      base::BindLambdaForTesting([kContext](Widget*) { return kContext; }));
  ElementEventWatcher shown(kTestElementID, kContext, ElementEventType::kShown);
  ElementEventWatcher hidden(kTestElementID, kContext,
                             ElementEventType::kHidden);
  auto* const view = widget_->SetContentsView(std::make_unique<View>());
  auto* const view2 = widget2_->SetContentsView(std::make_unique<View>());
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  // Add to first widget.
  auto* const button = view->AddChildView(std::move(button_ptr));
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(0, hidden.event_count());
  // Move to second widget.
  view2->AddChildView(button);
  EXPECT_EQ(2, shown.event_count());
  EXPECT_EQ(1, hidden.event_count());
  // Destroy the second widget.
  widget2_.reset();
  EXPECT_EQ(2, shown.event_count());
  EXPECT_EQ(2, hidden.event_count());
}

TEST_F(ElementTrackerTwoWidgetTest,
       OverrideContextCallbackOverridesContextSelectively) {
  const ui::ElementContext kContext{1};
  ElementTrackerViews::SetContextOverrideCallback(
      base::BindLambdaForTesting([this, kContext](Widget* widget) {
        return widget == widget_.get() ? kContext : ui::ElementContext();
      }));
  ElementEventWatcher shown(kTestElementID, kContext, ElementEventType::kShown);
  ElementEventWatcher hidden(kTestElementID, kContext,
                             ElementEventType::kHidden);
  ElementEventWatcher shown2(kTestElementID, context2(),
                             ElementEventType::kShown);
  ElementEventWatcher hidden2(kTestElementID, context2(),
                              ElementEventType::kHidden);
  auto* const view = widget_->SetContentsView(std::make_unique<View>());
  auto* const view2 = widget2_->SetContentsView(std::make_unique<View>());
  auto button_ptr = std::make_unique<LabelButton>();
  button_ptr->SetProperty(kElementIdentifierKey, kTestElementID);
  // Add to first widget.
  auto* const button = view->AddChildView(std::move(button_ptr));
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(0, hidden.event_count());
  EXPECT_EQ(0, shown2.event_count());
  EXPECT_EQ(0, hidden2.event_count());
  // Move to second widget.
  view2->AddChildView(button);
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(1, hidden.event_count());
  EXPECT_EQ(1, shown2.event_count());
  EXPECT_EQ(0, hidden2.event_count());
  // Destroy the second widget.
  widget2_.reset();
  EXPECT_EQ(1, shown.event_count());
  EXPECT_EQ(1, hidden.event_count());
  EXPECT_EQ(1, shown2.event_count());
  EXPECT_EQ(1, hidden2.event_count());
}

}  // namespace views
