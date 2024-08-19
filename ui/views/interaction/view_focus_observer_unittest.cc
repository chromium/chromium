// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/view_focus_observer.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views::test {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButton1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTextFieldId);
}  // namespace

class ViewFocusObserverTest : public InteractiveViewsTest {
 public:
  ViewFocusObserverTest() = default;
  ~ViewFocusObserverTest() override = default;

  void SetUp() override {
    InteractiveViewsTest::SetUp();
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(20, 20, 500, 100);
    widget_->Init(std::move(params));
    auto* const contents = widget_->SetContentsView(std::make_unique<View>());
    button1_ = contents->AddChildView(
        std::make_unique<LabelButton>(Button::PressedCallback(), u"Button1"));
    button1_->SetProperty(kElementIdentifierKey, kButton1Id);
    button2_ = contents->AddChildView(
        std::make_unique<LabelButton>(Button::PressedCallback(), u"Button2"));
    text_ = contents->AddChildView(std::make_unique<Textfield>());
    text_->SetProperty(kElementIdentifierKey, kTextFieldId);
    text_->SetDefaultWidthInChars(10);
    text_->SetAccessibleName(u"Textfield");
    auto* const layout =
        contents->SetLayoutManager(std::make_unique<FlexLayout>());
    layout->SetOrientation(LayoutOrientation::kHorizontal);
    layout->SetDefault(kFlexBehaviorKey,
                       FlexSpecification(MinimumFlexSizeRule::kPreferred,
                                         MaximumFlexSizeRule::kUnbounded));
    test::WidgetVisibleWaiter visible_waiter(widget_.get());
    widget_->Show();
    visible_waiter.Wait();

    SetContextWidget(widget_.get());
  }

  void TearDown() override {
    button1_ = nullptr;
    button2_ = nullptr;
    text_ = nullptr;
    widget_.reset();
    InteractiveViewsTest::TearDown();
  }

  auto Focus(View* view) {
    return Do([view]() { view->RequestFocus(); });
  }

 protected:
  raw_ptr<LabelButton> button1_ = nullptr;
  raw_ptr<LabelButton> button2_ = nullptr;
  raw_ptr<Textfield> text_ = nullptr;
  std::unique_ptr<Widget> widget_;
};

TEST_F(ViewFocusObserverTest, TracksFocus) {
  RunTestSequence(
      ObserveState(kCurrentFocusedView, widget_.get()),
      ObserveState(kCurrentFocusedViewId, widget_.get()), Focus(button1_),
      WaitForState(kCurrentFocusedView, button1_.get()),
      WaitForState(kCurrentFocusedViewId, kButton1Id), Focus(button2_),
      WaitForState(kCurrentFocusedView, button2_.get()),
      WaitForState(kCurrentFocusedViewId, ui::ElementIdentifier()),
      Focus(text_), WaitForState(kCurrentFocusedView, text_.get()),
      WaitForState(kCurrentFocusedViewId, kTextFieldId),
      Do([this]() { widget_->GetFocusManager()->ClearFocus(); }),
      WaitForState(kCurrentFocusedView,
                   testing::Matcher<View*>(testing::Not(testing::AnyOf(
                       button1_.get(), button2_.get(), text_.get())))),
      WaitForState(kCurrentFocusedViewId, ui::ElementIdentifier()));
}

}  // namespace views::test
