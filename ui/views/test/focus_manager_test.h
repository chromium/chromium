// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_FOCUS_MANAGER_TEST_H_
#define UI_VIEWS_TEST_FOCUS_MANAGER_TEST_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

class FocusChangeListener;

class FocusManagerTest : public ViewsTestBase, public WidgetDelegate {
 public:
  using FocusChangeReason = FocusManager::FocusChangeReason;

  FocusManagerTest();

  FocusManagerTest(const FocusManagerTest&) = delete;
  FocusManagerTest& operator=(const FocusManagerTest&) = delete;

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
  void RemoveFocusChangeListener(FocusChangeListener* listener);
  void AddWidgetFocusChangeListener(WidgetFocusChangeListener* listener);
  void RemoveWidgetFocusChangeListener(WidgetFocusChangeListener* listener);

  // For testing FocusManager::RotatePaneFocus().
  void SetAccessiblePanes(
      const std::vector<raw_ptr<View, VectorExperimental>>& panes);

 private:
  std::unique_ptr<Widget> widget_;
  raw_ptr<View> contents_view_ = nullptr;
  raw_ptr<FocusChangeListener> focus_change_listener_ = nullptr;
  raw_ptr<WidgetFocusChangeListener> widget_focus_change_listener_ = nullptr;
  std::vector<raw_ptr<View, VectorExperimental>> accessible_panes_;
};

using ViewPair = std::pair<View*, View*>;

// Use to record focus change notifications.
class TestFocusChangeListener : public FocusChangeListener {
 public:
  TestFocusChangeListener();

  TestFocusChangeListener(const TestFocusChangeListener&) = delete;
  TestFocusChangeListener& operator=(const TestFocusChangeListener&) = delete;

  ~TestFocusChangeListener() override;

  const std::vector<ViewPair>& focus_changes() const { return focus_changes_; }
  void ClearFocusChanges();

  // Overridden from FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

 private:
  // A vector of which views lost/gained focus.
  std::vector<ViewPair> focus_changes_;
};

// Use to record widget focus change notifications.
class TestWidgetFocusChangeListener : public WidgetFocusChangeListener {
 public:
  TestWidgetFocusChangeListener();

  TestWidgetFocusChangeListener(const TestWidgetFocusChangeListener&) = delete;
  TestWidgetFocusChangeListener& operator=(
      const TestWidgetFocusChangeListener&) = delete;

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
};

}  // namespace views

#endif  // UI_VIEWS_TEST_FOCUS_MANAGER_TEST_H_
