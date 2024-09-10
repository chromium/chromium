// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/focus_manager_test.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// FocusManagerTest

FocusManagerTest::FocusManagerTest() : contents_view_(new View) {}

FocusManagerTest::~FocusManagerTest() = default;

FocusManager* FocusManagerTest::GetFocusManager() {
  return GetWidget()->GetFocusManager();
}

void FocusManagerTest::SetUp() {
  ViewsTestBase::SetUp();

  widget_ = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget_->Init(std::move(params));

  InitContentView();
  widget_->Show();
}

void FocusManagerTest::TearDown() {
  if (focus_change_listener_)
    GetFocusManager()->RemoveFocusChangeListener(focus_change_listener_);
  if (widget_focus_change_listener_) {
    WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(
        widget_focus_change_listener_);
  }
  GetWidget()->Close();
  contents_view_ = nullptr;

  // Flush the message loop to make application verifiers happy.
  RunPendingMessages();
  contents_view_ = nullptr;
  focus_change_listener_ = nullptr;
  widget_focus_change_listener_ = nullptr;
  accessible_panes_.clear();
  widget_.reset();
  ViewsTestBase::TearDown();
}

View* FocusManagerTest::GetContentsView() {
  return contents_view_;
}

Widget* FocusManagerTest::GetWidget() {
  return contents_view_->GetWidget();
}

const Widget* FocusManagerTest::GetWidget() const {
  return contents_view_->GetWidget();
}

void FocusManagerTest::GetAccessiblePanes(std::vector<View*>* panes) {
  base::ranges::copy(accessible_panes_, std::back_inserter(*panes));
}

void FocusManagerTest::InitContentView() {}

void FocusManagerTest::AddFocusChangeListener(FocusChangeListener* listener) {
  ASSERT_FALSE(focus_change_listener_);
  focus_change_listener_ = listener;
  GetFocusManager()->AddFocusChangeListener(listener);
}

void FocusManagerTest::RemoveFocusChangeListener(
    FocusChangeListener* listener) {
  ASSERT_TRUE(focus_change_listener_);
  ASSERT_EQ(focus_change_listener_, listener);
  GetFocusManager()->RemoveFocusChangeListener(listener);
  focus_change_listener_ = nullptr;
}

void FocusManagerTest::AddWidgetFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  ASSERT_FALSE(widget_focus_change_listener_);
  widget_focus_change_listener_ = listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(listener);
}

void FocusManagerTest::RemoveWidgetFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  ASSERT_TRUE(widget_focus_change_listener_);
  ASSERT_EQ(widget_focus_change_listener_, listener);
  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(listener);
  widget_focus_change_listener_ = nullptr;
}

void FocusManagerTest::SetAccessiblePanes(
    const std::vector<raw_ptr<View, VectorExperimental>>& panes) {
  accessible_panes_ = panes;
}

////////////////////////////////////////////////////////////////////////////////
// TestFocusChangeListener

TestFocusChangeListener::TestFocusChangeListener() = default;

TestFocusChangeListener::~TestFocusChangeListener() = default;

void TestFocusChangeListener::OnWillChangeFocus(View* focused_before,
                                                View* focused_now) {
  focus_changes_.emplace_back(focused_before, focused_now);
}
void TestFocusChangeListener::OnDidChangeFocus(View* focused_before,
                                               View* focused_now) {}

void TestFocusChangeListener::ClearFocusChanges() {
  focus_changes_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TestWidgetFocusChangeListener

TestWidgetFocusChangeListener::TestWidgetFocusChangeListener() = default;

TestWidgetFocusChangeListener::~TestWidgetFocusChangeListener() = default;

void TestWidgetFocusChangeListener::ClearFocusChanges() {
  focus_changes_.clear();
}

void TestWidgetFocusChangeListener::OnNativeFocusChanged(
    gfx::NativeView focused_now) {
  focus_changes_.push_back(focused_now);
}

}  // namespace views
