// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/focus_manager_test.h"

#include <algorithm>

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

  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget->Init(std::move(params));

  InitContentView();
  widget->Show();
}

void FocusManagerTest::TearDown() {
  if (focus_change_listener_)
    GetFocusManager()->RemoveFocusChangeListener(focus_change_listener_);
  if (widget_focus_change_listener_) {
    WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(
        widget_focus_change_listener_);
  }
  GetWidget()->Close();

  // Flush the message loop to make application verifiers happy.
  RunPendingMessages();
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
  std::copy(accessible_panes_.begin(), accessible_panes_.end(),
            std::back_inserter(*panes));
}

void FocusManagerTest::InitContentView() {}

void FocusManagerTest::AddFocusChangeListener(FocusChangeListener* listener) {
  ASSERT_FALSE(focus_change_listener_);
  focus_change_listener_ = listener;
  GetFocusManager()->AddFocusChangeListener(listener);
}

void FocusManagerTest::AddWidgetFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  ASSERT_FALSE(widget_focus_change_listener_);
  widget_focus_change_listener_ = listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(listener);
}

void FocusManagerTest::SetAccessiblePanes(const std::vector<View*>& panes) {
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
