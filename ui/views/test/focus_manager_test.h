// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_FOCUS_MANAGER_TEST_H_
#define UI_VIEWS_TEST_FOCUS_MANAGER_TEST_H_

#include "ui/views/focus/focus_manager.h"

#include <utility>
#include <vector>

#include "base/macros.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

class FocusChangeListener;

class FocusManagerTest : public ViewsTestBase, public WidgetDelegate {
 public:
  using FocusChangeReason = FocusManager::FocusChangeReason;

  FocusManagerTest();
  ~FocusManagerTest() override;

  // Convenience to obtain the focus manager for the test's hosting widget.
  FocusManager* GetFocusManager();

  // ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

  // WidgetDelegate:
  View* GetContentsView() override;
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  void GetAccessiblePanes(std::vector<View*>* panes) override;

 protected:
  // Called after the Widget is initialized and the content view is added.
  // Override to add controls to the layout.
  virtual void InitContentView();

  void AddFocusChangeListener(FocusChangeListener* listener);
  void AddWidgetFocusChangeListener(WidgetFocusChangeListener* listener);

  // For testing FocusManager::RotatePaneFocus().
  void SetAccessiblePanes(const std::vector<View*>& panes);

 private:
  View* contents_view_;
  FocusChangeListener* focus_change_listener_ = nullptr;
  WidgetFocusChangeListener* widget_focus_change_listener_ = nullptr;
  std::vector<View*> accessible_panes_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerTest);
};

using ViewPair = std::pair<View*, View*>;

// Use to record focus change notifications.
class TestFocusChangeListener : public FocusChangeListener {
 public:
  TestFocusChangeListener();
  ~TestFocusChangeListener() override;

  const std::vector<ViewPair>& focus_changes() const { return focus_changes_; }
  void ClearFocusChanges();

  // Overridden from FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

 private:
  // A vector of which views lost/gained focus.
  std::vector<ViewPair> focus_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusChangeListener);
};

// Use to record widget focus change notifications.
class TestWidgetFocusChangeListener : public WidgetFocusChangeListener {
 public:
  TestWidgetFocusChangeListener();
  ~TestWidgetFocusChangeListener() override;

  const std::vector<gfx::NativeView>& focus_changes() const {
    return focus_changes_;
  }
  void ClearFocusChanges();

  // Overridden from WidgetFocusChangeListener:
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

 private:
  // Parameter received via OnNativeFocusChanged in oldest-to-newest-received
  // order.
  std::vector<gfx::NativeView> focus_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetFocusChangeListener);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_FOCUS_MANAGER_TEST_H_
