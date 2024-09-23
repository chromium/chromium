// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_manager.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/buildflags.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/focus/focus_manager_delegate.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/test_platform_native_widget.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/focus_client.h"
#include "ui/views/widget/native_widget_aura.h"
#endif  // USE_AURA

namespace views {

enum FocusTestEventType { ON_FOCUS = 0, ON_BLUR };

struct FocusTestEvent {
  FocusTestEventType type;
  int view_id;
  FocusManager::FocusChangeReason focus_change_reason;
};

class FocusTestEventList : public base::RefCounted<FocusTestEventList> {
 public:
  std::vector<FocusTestEvent> vec;

 private:
  friend class base::RefCounted<FocusTestEventList>;
  ~FocusTestEventList() = default;
};

class SimpleTestView : public View {
  METADATA_HEADER(SimpleTestView, View)

 public:
  SimpleTestView(scoped_refptr<FocusTestEventList> event_list, int view_id)
      : event_list_(std::move(event_list)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    set_suppress_default_focus_handling();
    SetID(view_id);
  }

  SimpleTestView(const SimpleTestView&) = delete;
  SimpleTestView& operator=(const SimpleTestView&) = delete;

  void OnFocus() override {
    event_list_->vec.push_back({
        ON_FOCUS,
        GetID(),
        GetFocusManager()->focus_change_reason(),
    });
  }

  void OnBlur() override {
    event_list_->vec.push_back({
        ON_BLUR,
        GetID(),
        GetFocusManager()->focus_change_reason(),
    });
  }

 private:
  const scoped_refptr<FocusTestEventList> event_list_;
};

BEGIN_METADATA(SimpleTestView)
END_METADATA

// Tests that the appropriate Focus related methods are called when a View
// gets/loses focus.
TEST_F(FocusManagerTest, ViewFocusCallbacks) {
  auto event_list = base::MakeRefCounted<FocusTestEventList>();
  const int kView1ID = 1;
  const int kView2ID = 2;
  SimpleTestView* view1 = new SimpleTestView(event_list, kView1ID);
  SimpleTestView* view2 = new SimpleTestView(event_list, kView2ID);
  GetContentsView()->AddChildView(view1);
  GetContentsView()->AddChildView(view2);

  view1->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(event_list->vec.size()));
  EXPECT_EQ(ON_FOCUS, event_list->vec[0].type);
  EXPECT_EQ(kView1ID, event_list->vec[0].view_id);
  EXPECT_EQ(FocusChangeReason::kDirectFocusChange,
            event_list->vec[0].focus_change_reason);

  event_list->vec.clear();
  view2->RequestFocus();
  ASSERT_EQ(2, static_cast<int>(event_list->vec.size()));
  EXPECT_EQ(ON_BLUR, event_list->vec[0].type);
  EXPECT_EQ(kView1ID, event_list->vec[0].view_id);
  EXPECT_EQ(ON_FOCUS, event_list->vec[1].type);
  EXPECT_EQ(kView2ID, event_list->vec[1].view_id);
  EXPECT_EQ(FocusChangeReason::kDirectFocusChange,
            event_list->vec[0].focus_change_reason);
  EXPECT_EQ(FocusChangeReason::kDirectFocusChange,
            event_list->vec[1].focus_change_reason);

  event_list->vec.clear();
  GetFocusManager()->ClearFocus();
  ASSERT_EQ(1, static_cast<int>(event_list->vec.size()));
  EXPECT_EQ(ON_BLUR, event_list->vec[0].type);
  EXPECT_EQ(kView2ID, event_list->vec[0].view_id);
  EXPECT_EQ(FocusChangeReason::kDirectFocusChange,
            event_list->vec[0].focus_change_reason);
}

TEST_F(FocusManagerTest, FocusChangeListener) {
  View* view1 = new View();
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  View* view2 = new View();
  view2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  GetContentsView()->AddChildView(view1);
  GetContentsView()->AddChildView(view2);

  TestFocusChangeListener listener;
  AddFocusChangeListener(&listener);

  // Required for VS2010:
  // http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  views::View* null_view = nullptr;

  view1->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(null_view, view1));
  listener.ClearFocusChanges();

  view2->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view1, view2));
  listener.ClearFocusChanges();

  GetFocusManager()->ClearFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view2, null_view));

  RemoveFocusChangeListener(&listener);
}

TEST_F(FocusManagerTest, WidgetFocusChangeListener) {
  // First, ensure the simulator is aware of the Widget created in SetUp() being
  // currently active.
  test::WidgetTest::SimulateNativeActivate(GetWidget());

  TestWidgetFocusChangeListener widget_listener;
  AddWidgetFocusChangeListener(&widget_listener);

  Widget::InitParams params1 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params1.bounds = gfx::Rect(10, 10, 100, 100);
  params1.parent = GetWidget()->GetNativeView();
  auto widget1 = std::make_unique<Widget>();
  widget1->Init(std::move(params1));
  widget1->Show();

  Widget::InitParams params2 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params2.bounds = gfx::Rect(10, 10, 100, 100);
  params2.parent = GetWidget()->GetNativeView();
  auto widget2 = std::make_unique<Widget>();
  widget2->Init(std::move(params2));
  widget2->Show();

  widget_listener.ClearFocusChanges();
  gfx::NativeView native_view1 = widget1->GetNativeView();
  test::WidgetTest::SimulateNativeActivate(widget1.get());
  ASSERT_EQ(2u, widget_listener.focus_changes().size());
  EXPECT_EQ(gfx::NativeView(), widget_listener.focus_changes()[0]);
  EXPECT_EQ(native_view1, widget_listener.focus_changes()[1]);

  widget_listener.ClearFocusChanges();
  gfx::NativeView native_view2 = widget2->GetNativeView();
  test::WidgetTest::SimulateNativeActivate(widget2.get());
  ASSERT_EQ(2u, widget_listener.focus_changes().size());
  EXPECT_EQ(gfx::NativeView(), widget_listener.focus_changes()[0]);
  EXPECT_EQ(native_view2, widget_listener.focus_changes()[1]);

  RemoveWidgetFocusChangeListener(&widget_listener);
}

TEST_F(FocusManagerTest, CallsNormalAcceleratorTarget) {
  FocusManager* focus_manager = GetFocusManager();
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  ui::Accelerator escape_accelerator(ui::VKEY_ESCAPE, ui::EF_NONE);

  ui::TestAcceleratorTarget return_target(true);
  ui::TestAcceleratorTarget escape_target(true);
  EXPECT_EQ(return_target.accelerator_count(), 0);
  EXPECT_EQ(escape_target.accelerator_count(), 0);

  // Register targets.
  focus_manager->RegisterAccelerator(return_accelerator,
                                     ui::AcceleratorManager::kNormalPriority,
                                     &return_target);
  focus_manager->RegisterAccelerator(escape_accelerator,
                                     ui::AcceleratorManager::kNormalPriority,
                                     &escape_target);

  // Hitting the return key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(escape_target.accelerator_count(), 0);

  // Hitting the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(escape_target.accelerator_count(), 1);

  // Register another target for the return key.
  ui::TestAcceleratorTarget return_target2(true);
  EXPECT_EQ(return_target2.accelerator_count(), 0);
  focus_manager->RegisterAccelerator(return_accelerator,
                                     ui::AcceleratorManager::kNormalPriority,
                                     &return_target2);

  // Hitting the return key; return_target2 has the priority.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(return_target2.accelerator_count(), 1);

  // Register a target that does not process the accelerator event.
  ui::TestAcceleratorTarget return_target3(false);
  EXPECT_EQ(return_target3.accelerator_count(), 0);
  focus_manager->RegisterAccelerator(return_accelerator,
                                     ui::AcceleratorManager::kNormalPriority,
                                     &return_target3);
  // Hitting the return key.
  // Since the event handler of return_target3 returns false, return_target2
  // should be called too.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 1);

  // Unregister return_target2.
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target2);

  // Hitting the return key. return_target3 and return_target should be called.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 2);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 2);

  // Unregister targets.
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target);
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target3);
  focus_manager->UnregisterAccelerator(escape_accelerator, &escape_target);

  // Hitting the return key and the escape key. Nothing should happen.
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 2);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 2);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target.accelerator_count(), 1);
}

TEST_F(FocusManagerTest, HighPriorityHandlers) {
  FocusManager* focus_manager = GetFocusManager();
  ui::Accelerator escape_accelerator(ui::VKEY_ESCAPE, ui::EF_NONE);

  ui::TestAcceleratorTarget escape_target_high(true);
  ui::TestAcceleratorTarget escape_target_normal(true);
  EXPECT_EQ(escape_target_high.accelerator_count(), 0);
  EXPECT_EQ(escape_target_normal.accelerator_count(), 0);
  EXPECT_FALSE(focus_manager->HasPriorityHandler(escape_accelerator));

  // Register high priority target.
  focus_manager->RegisterAccelerator(escape_accelerator,
                                     ui::AcceleratorManager::kHighPriority,
                                     &escape_target_high);
  EXPECT_TRUE(focus_manager->HasPriorityHandler(escape_accelerator));

  // Hit the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target_high.accelerator_count(), 1);
  EXPECT_EQ(escape_target_normal.accelerator_count(), 0);

  // Add a normal priority target and make sure it doesn't see the key.
  focus_manager->RegisterAccelerator(escape_accelerator,
                                     ui::AcceleratorManager::kNormalPriority,
                                     &escape_target_normal);

  // Checks if the correct target is registered (same as before, the high
  // priority one).
  EXPECT_TRUE(focus_manager->HasPriorityHandler(escape_accelerator));

  // Hit the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target_high.accelerator_count(), 2);
  EXPECT_EQ(escape_target_normal.accelerator_count(), 0);

  // Unregister the high priority accelerator.
  focus_manager->UnregisterAccelerator(escape_accelerator, &escape_target_high);
  EXPECT_FALSE(focus_manager->HasPriorityHandler(escape_accelerator));

  // Hit the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target_high.accelerator_count(), 2);
  EXPECT_EQ(escape_target_normal.accelerator_count(), 1);

  // Add the high priority target back and make sure it starts seeing the key.
  focus_manager->RegisterAccelerator(escape_accelerator,
                                     ui::AcceleratorManager::kHighPriority,
                                     &escape_target_high);
  EXPECT_TRUE(focus_manager->HasPriorityHandler(escape_accelerator));

  // Hit the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target_high.accelerator_count(), 3);
  EXPECT_EQ(escape_target_normal.accelerator_count(), 1);

  // Unregister the normal priority accelerator.
  focus_manager->UnregisterAccelerator(escape_accelerator,
                                       &escape_target_normal);
  EXPECT_TRUE(focus_manager->HasPriorityHandler(escape_accelerator));

  // Hit the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target_high.accelerator_count(), 4);
  EXPECT_EQ(escape_target_normal.accelerator_count(), 1);

  // Unregister the high priority accelerator.
  focus_manager->UnregisterAccelerator(escape_accelerator, &escape_target_high);
  EXPECT_FALSE(focus_manager->HasPriorityHandler(escape_accelerator));

  // Hit the escape key (no change, no targets registered).
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target_high.accelerator_count(), 4);
  EXPECT_EQ(escape_target_normal.accelerator_count(), 1);
}

TEST_F(FocusManagerTest, CallsEnabledAcceleratorTargetsOnly) {
  FocusManager* focus_manager = GetFocusManager();
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);

  ui::TestAcceleratorTarget return_target1(true);
  ui::TestAcceleratorTarget return_target2(true);

  focus_manager->RegisterAccelerator(return_accelerator,
                                     ui::AcceleratorManager::kNormalPriority,
                                     &return_target1);
  focus_manager->RegisterAccelerator(return_accelerator,
                                     ui::AcceleratorManager::kNormalPriority,
                                     &return_target2);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(0, return_target1.accelerator_count());
  EXPECT_EQ(1, return_target2.accelerator_count());

  // If CanHandleAccelerators() return false, FocusManager shouldn't call
  // AcceleratorPressed().
  return_target2.set_can_handle_accelerators(false);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, return_target1.accelerator_count());
  EXPECT_EQ(1, return_target2.accelerator_count());

  // If no accelerator targets are enabled, ProcessAccelerator() should fail.
  return_target1.set_can_handle_accelerators(false);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, return_target1.accelerator_count());
  EXPECT_EQ(1, return_target2.accelerator_count());

  // Enabling the target again causes the accelerators to be processed again.
  return_target1.set_can_handle_accelerators(true);
  return_target2.set_can_handle_accelerators(true);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, return_target1.accelerator_count());
  EXPECT_EQ(2, return_target2.accelerator_count());
}

// Unregisters itself when its accelerator is invoked.
class SelfUnregisteringAcceleratorTarget : public ui::TestAcceleratorTarget {
 public:
  SelfUnregisteringAcceleratorTarget(const ui::Accelerator& accelerator,
                                     FocusManager* focus_manager)
      : accelerator_(accelerator), focus_manager_(focus_manager) {}

  SelfUnregisteringAcceleratorTarget(
      const SelfUnregisteringAcceleratorTarget&) = delete;
  SelfUnregisteringAcceleratorTarget& operator=(
      const SelfUnregisteringAcceleratorTarget&) = delete;

  // ui::TestAcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    focus_manager_->UnregisterAccelerator(accelerator, this);
    return ui::TestAcceleratorTarget::AcceleratorPressed(accelerator);
  }

 private:
  ui::Accelerator accelerator_;
  raw_ptr<FocusManager> focus_manager_;
};

TEST_F(FocusManagerTest, CallsSelfDeletingAcceleratorTarget) {
  FocusManager* focus_manager = GetFocusManager();
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  SelfUnregisteringAcceleratorTarget target(return_accelerator, focus_manager);
  EXPECT_EQ(target.accelerator_count(), 0);

  // Register the target.
  focus_manager->RegisterAccelerator(
      return_accelerator, ui::AcceleratorManager::kNormalPriority, &target);

  // Hitting the return key. The target will be unregistered.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(target.accelerator_count(), 1);

  // Hitting the return key again; nothing should happen.
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(target.accelerator_count(), 1);
}

TEST_F(FocusManagerTest, SuspendAccelerators) {
  const ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::EF_NONE);
  ui::Accelerator accelerator(event.key_code(), event.flags());
  ui::TestAcceleratorTarget target(true);
  FocusManager* focus_manager = GetFocusManager();
  focus_manager->RegisterAccelerator(
      accelerator, ui::AcceleratorManager::kNormalPriority, &target);

  focus_manager->set_shortcut_handling_suspended(true);
  EXPECT_TRUE(focus_manager->OnKeyEvent(event));
  EXPECT_EQ(0, target.accelerator_count());

  focus_manager->set_shortcut_handling_suspended(false);
  EXPECT_FALSE(focus_manager->OnKeyEvent(event));
  EXPECT_EQ(1, target.accelerator_count());
}

namespace {

class FocusInAboutToRequestFocusFromTabTraversalView : public View {
  METADATA_HEADER(FocusInAboutToRequestFocusFromTabTraversalView, View)

 public:
  FocusInAboutToRequestFocusFromTabTraversalView() = default;

  FocusInAboutToRequestFocusFromTabTraversalView(
      const FocusInAboutToRequestFocusFromTabTraversalView&) = delete;
  FocusInAboutToRequestFocusFromTabTraversalView& operator=(
      const FocusInAboutToRequestFocusFromTabTraversalView&) = delete;

  void set_view_to_focus(View* view) { view_to_focus_ = view; }

  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    view_to_focus_->RequestFocus();
  }

 private:
  raw_ptr<views::View> view_to_focus_ = nullptr;
};

BEGIN_METADATA(FocusInAboutToRequestFocusFromTabTraversalView)
END_METADATA

}  // namespace

// Verifies a focus change done during a call to
// AboutToRequestFocusFromTabTraversal() is honored.
TEST_F(FocusManagerTest, FocusInAboutToRequestFocusFromTabTraversal) {
  // Create 3 views.
  views::View* v1 = new View;
  v1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  GetContentsView()->AddChildView(v1);

  FocusInAboutToRequestFocusFromTabTraversalView* v2 =
      new FocusInAboutToRequestFocusFromTabTraversalView;
  v2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  v2->set_view_to_focus(v1);
  GetContentsView()->AddChildView(v2);

  views::View* v3 = new View;
  v3->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  GetContentsView()->AddChildView(v3);

  // Focus the third view and advances to the second. The second view's
  // implementation of AboutToRequestFocusFromTabTraversal() focuses the first.
  v3->RequestFocus();
  GetWidget()->GetFocusManager()->AdvanceFocus(true);
  EXPECT_TRUE(v1->HasFocus());

  v2->set_view_to_focus(nullptr);
}

TEST_F(FocusManagerTest, RotateFocus) {
  views::AccessiblePaneView* pane1 = new AccessiblePaneView();
  GetContentsView()->AddChildView(pane1);

  views::View* v1 = new View;
  v1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  pane1->AddChildView(v1);

  views::View* v2 = new View;
  v2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  pane1->AddChildView(v2);

  views::AccessiblePaneView* pane2 = new AccessiblePaneView();
  GetContentsView()->AddChildView(pane2);

  views::View* v3 = new View;
  v3->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  pane2->AddChildView(v3);

  views::View* v4 = new View;
  v4->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  pane2->AddChildView(v4);

  std::vector<raw_ptr<views::View, VectorExperimental>> panes;
  panes.push_back(pane1);
  panes.push_back(pane2);
  SetAccessiblePanes(panes);

  FocusManager* focus_manager = GetWidget()->GetFocusManager();

  // Advance forwards. Focus should stay trapped within each pane.
  using Direction = FocusManager::Direction;
  using FocusCycleWrapping = FocusManager::FocusCycleWrapping;
  EXPECT_TRUE(focus_manager->RotatePaneFocus(Direction::kForward,
                                             FocusCycleWrapping::kEnabled));
  EXPECT_EQ(v1, focus_manager->GetFocusedView());
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(v2, focus_manager->GetFocusedView());
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(v1, focus_manager->GetFocusedView());

  EXPECT_TRUE(focus_manager->RotatePaneFocus(Direction::kForward,
                                             FocusCycleWrapping::kEnabled));
  EXPECT_EQ(v3, focus_manager->GetFocusedView());
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(v4, focus_manager->GetFocusedView());
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(v3, focus_manager->GetFocusedView());
}

// Verifies the stored focus view tracks the focused view.
TEST_F(FocusManagerTest, ImplicitlyStoresFocus) {
  views::View* v1 = new View;
  v1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  GetContentsView()->AddChildView(v1);

  views::View* v2 = new View;
  v2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  GetContentsView()->AddChildView(v2);

  // Verify a focus request on |v1| implicitly updates the stored focus view.
  v1->RequestFocus();
  EXPECT_TRUE(v1->HasFocus());
  EXPECT_EQ(v1, GetWidget()->GetFocusManager()->GetStoredFocusView());

  // Verify a focus request on |v2| implicitly updates the stored focus view.
  v2->RequestFocus();
  EXPECT_TRUE(v2->HasFocus());
  EXPECT_EQ(v2, GetWidget()->GetFocusManager()->GetStoredFocusView());
}

namespace {

class FocusManagerArrowKeyTraversalTest
    : public FocusManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  FocusManagerArrowKeyTraversalTest() = default;
  FocusManagerArrowKeyTraversalTest(const FocusManagerArrowKeyTraversalTest&) =
      delete;
  FocusManagerArrowKeyTraversalTest& operator=(
      const FocusManagerArrowKeyTraversalTest&) = delete;
  ~FocusManagerArrowKeyTraversalTest() override = default;

  // FocusManagerTest overrides:
  void SetUp() override {
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      is_rtl_ = GetParam();
      if (is_rtl_)
        base::i18n::SetICUDefaultLocale("he");
    }

    FocusManagerTest::SetUp();
    previous_arrow_key_traversal_enabled_ =
        FocusManager::arrow_key_traversal_enabled();
  }

  void TearDown() override {
    FocusManager::set_arrow_key_traversal_enabled(
        previous_arrow_key_traversal_enabled_);
    FocusManagerTest::TearDown();
  }

  bool is_rtl_ = false;

 private:
  // Restores the locale to default when the destructor is called.
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;

  bool previous_arrow_key_traversal_enabled_ = false;
};

// Instantiate the Boolean which is used to toggle RTL in
// the parameterized tests.
INSTANTIATE_TEST_SUITE_P(All,
                         FocusManagerArrowKeyTraversalTest,
                         testing::Bool());

}  // namespace

TEST_P(FocusManagerArrowKeyTraversalTest, ArrowKeyTraversal) {
  FocusManager* focus_manager = GetFocusManager();
  const ui::KeyEvent left_key(ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                              ui::EF_NONE);
  const ui::KeyEvent right_key(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                               ui::EF_NONE);
  const ui::KeyEvent up_key(ui::EventType::kKeyPressed, ui::VKEY_UP,
                            ui::EF_NONE);
  const ui::KeyEvent down_key(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                              ui::EF_NONE);

  std::vector<views::View*> v;
  for (size_t i = 0; i < 2; ++i) {
    views::View* view = new View;
    view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
    GetContentsView()->AddChildView(view);
    v.push_back(view);
  }

  // Arrow key traversal is off and arrow key does not change focus.
  FocusManager::set_arrow_key_traversal_enabled(false);
  v[0]->RequestFocus();
  focus_manager->OnKeyEvent(right_key);
  EXPECT_EQ(v[0], focus_manager->GetFocusedView());
  focus_manager->OnKeyEvent(left_key);
  EXPECT_EQ(v[0], focus_manager->GetFocusedView());
  focus_manager->OnKeyEvent(down_key);
  EXPECT_EQ(v[0], focus_manager->GetFocusedView());
  focus_manager->OnKeyEvent(up_key);
  EXPECT_EQ(v[0], focus_manager->GetFocusedView());

  // Turn on arrow key traversal.
  FocusManager::set_arrow_key_traversal_enabled(true);
  v[0]->RequestFocus();
  focus_manager->OnKeyEvent(is_rtl_ ? left_key : right_key);
  EXPECT_EQ(v[1], focus_manager->GetFocusedView());
  focus_manager->OnKeyEvent(is_rtl_ ? right_key : left_key);
  EXPECT_EQ(v[0], focus_manager->GetFocusedView());
  focus_manager->OnKeyEvent(down_key);
  EXPECT_EQ(v[1], focus_manager->GetFocusedView());
  focus_manager->OnKeyEvent(up_key);
  EXPECT_EQ(v[0], focus_manager->GetFocusedView());
}

TEST_F(FocusManagerTest, SkipViewsInArrowKeyTraversal) {
  FocusManager* focus_manager = GetFocusManager();

  // Test the focus on the views which are under same group.
  std::vector<views::View*> v;
  for (size_t i = 0; i < 5; ++i) {
    auto* view =
        GetContentsView()->AddChildView(std::make_unique<views::View>());
    view->SetGroup(12345);
    // Testing both kind of focuses (Always focusable and only accessibility
    // focusable).
    view->SetFocusBehavior((i == 0 || i == 4)
                               ? View::FocusBehavior::ALWAYS
                               : View::FocusBehavior::ACCESSIBLE_ONLY);
    v.push_back(view);
  }

  // Disable view at index 1, and hide view at index 3.
  v[1]->SetEnabled(false);
  v[3]->SetVisible(false);

  // Start with focusing on the first view which is always accessible.
  v[0]->RequestFocus();
  EXPECT_EQ(v[0], focus_manager->GetFocusedView());

  // Check that focus does not go to a disabled/hidden view.
  const ui::KeyEvent right_key(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                               ui::EF_NONE);
  focus_manager->OnKeyEvent(right_key);
  EXPECT_EQ(v[2], focus_manager->GetFocusedView());

  focus_manager->OnKeyEvent(right_key);
  EXPECT_EQ(v[4], focus_manager->GetFocusedView());

  const ui::KeyEvent left_key(ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                              ui::EF_NONE);
  focus_manager->OnKeyEvent(left_key);
  EXPECT_EQ(v[2], focus_manager->GetFocusedView());

  focus_manager->OnKeyEvent(left_key);
  EXPECT_EQ(v[0], focus_manager->GetFocusedView());

  // On making the views visible/enabled, the focus should start appearing.
  v[1]->SetEnabled(true);
  v[3]->SetVisible(true);

  focus_manager->OnKeyEvent(right_key);
  EXPECT_EQ(v[1], focus_manager->GetFocusedView());

  focus_manager->OnKeyEvent(right_key);
  EXPECT_EQ(v[2], focus_manager->GetFocusedView());

  focus_manager->OnKeyEvent(right_key);
  EXPECT_EQ(v[3], focus_manager->GetFocusedView());
}

TEST_F(FocusManagerTest, StoreFocusedView) {
  auto event_list = base::MakeRefCounted<FocusTestEventList>();
  const int kView1ID = 1;
  SimpleTestView* view = new SimpleTestView(event_list, kView1ID);

  // Add view to the view hierarchy and make it focusable.
  GetWidget()->GetRootView()->AddChildView(view);
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  GetFocusManager()->SetFocusedView(view);
  GetFocusManager()->StoreFocusedView(false);
  EXPECT_EQ(nullptr, GetFocusManager()->GetFocusedView());
  EXPECT_TRUE(GetFocusManager()->RestoreFocusedView());
  EXPECT_EQ(view, GetFocusManager()->GetStoredFocusView());
  ASSERT_EQ(3, static_cast<int>(event_list->vec.size()));
  EXPECT_EQ(ON_FOCUS, event_list->vec[0].type);
  EXPECT_EQ(kView1ID, event_list->vec[0].view_id);
  EXPECT_EQ(FocusChangeReason::kDirectFocusChange,
            event_list->vec[0].focus_change_reason);
  EXPECT_EQ(ON_BLUR, event_list->vec[1].type);
  EXPECT_EQ(kView1ID, event_list->vec[1].view_id);
  EXPECT_EQ(FocusChangeReason::kDirectFocusChange,
            event_list->vec[1].focus_change_reason);
  EXPECT_EQ(ON_FOCUS, event_list->vec[2].type);
  EXPECT_EQ(kView1ID, event_list->vec[2].view_id);
  EXPECT_EQ(FocusChangeReason::kFocusRestore,
            event_list->vec[2].focus_change_reason);

  // Repeat with |true|.
  event_list->vec.clear();
  GetFocusManager()->SetFocusedView(view);
  GetFocusManager()->StoreFocusedView(true);
  EXPECT_EQ(nullptr, GetFocusManager()->GetFocusedView());
  EXPECT_TRUE(GetFocusManager()->RestoreFocusedView());
  EXPECT_EQ(view, GetFocusManager()->GetStoredFocusView());
  ASSERT_EQ(2, static_cast<int>(event_list->vec.size()));
  EXPECT_EQ(ON_BLUR, event_list->vec[0].type);
  EXPECT_EQ(kView1ID, event_list->vec[0].view_id);
  EXPECT_EQ(FocusChangeReason::kDirectFocusChange,
            event_list->vec[0].focus_change_reason);
  EXPECT_EQ(ON_FOCUS, event_list->vec[1].type);
  EXPECT_EQ(kView1ID, event_list->vec[1].view_id);
  EXPECT_EQ(FocusChangeReason::kFocusRestore,
            event_list->vec[1].focus_change_reason);

  // Necessary for clean teardown.
  GetFocusManager()->ClearFocus();
}

#if BUILDFLAG(IS_MAC)
// Test that the correct view is restored if full keyboard access is changed.
TEST_F(FocusManagerTest, StoreFocusedViewFullKeyboardAccess) {
  View* view1 = new View;
  View* view2 = new View;
  View* view3 = new View;

  // Make view1 focusable in accessibility mode, view2 not focusable and view3
  // always focusable.
  view1->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  view2->SetFocusBehavior(View::FocusBehavior::NEVER);
  view3->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  // Add views to the view hierarchy
  GetWidget()->GetRootView()->AddChildView(view1);
  GetWidget()->GetRootView()->AddChildView(view2);
  GetWidget()->GetRootView()->AddChildView(view3);

  view1->RequestFocus();
  EXPECT_EQ(view1, GetFocusManager()->GetFocusedView());
  GetFocusManager()->StoreFocusedView(true);
  EXPECT_EQ(nullptr, GetFocusManager()->GetFocusedView());

  // Turn off full keyboard access mode and restore focused view. Since view1 is
  // no longer focusable, view3 should have focus.
  GetFocusManager()->SetKeyboardAccessible(false);
  EXPECT_FALSE(GetFocusManager()->RestoreFocusedView());
  EXPECT_EQ(view3, GetFocusManager()->GetFocusedView());

  GetFocusManager()->StoreFocusedView(false);
  EXPECT_EQ(nullptr, GetFocusManager()->GetFocusedView());

  // Turn on full keyboard access mode and restore focused view. Since view3 is
  // still focusable, view3 should have focus.
  GetFocusManager()->SetKeyboardAccessible(true);
  EXPECT_TRUE(GetFocusManager()->RestoreFocusedView());
  EXPECT_EQ(view3, GetFocusManager()->GetFocusedView());
}

// Test that View::RequestFocus() respects full keyboard access mode.
TEST_F(FocusManagerTest, RequestFocus) {
  View* view1 = new View();
  View* view2 = new View();

  // Make view1 always focusable, view2 only focusable in accessibility mode.
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view2->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);

  // Adds views to the view hierarchy.
  GetWidget()->GetRootView()->AddChildView(view1);
  GetWidget()->GetRootView()->AddChildView(view2);

  // Verify view1 can always get focus via View::RequestFocus, while view2 can
  // only get focus in full keyboard accessibility mode.
  EXPECT_TRUE(GetFocusManager()->keyboard_accessible());
  view1->RequestFocus();
  EXPECT_EQ(view1, GetFocusManager()->GetFocusedView());
  view2->RequestFocus();
  EXPECT_EQ(view2, GetFocusManager()->GetFocusedView());

  // Toggle full keyboard accessibility.
  GetFocusManager()->SetKeyboardAccessible(false);

  GetFocusManager()->ClearFocus();
  EXPECT_NE(view1, GetFocusManager()->GetFocusedView());
  view1->RequestFocus();
  EXPECT_EQ(view1, GetFocusManager()->GetFocusedView());
  view2->RequestFocus();
  EXPECT_EQ(view1, GetFocusManager()->GetFocusedView());
}

#endif

namespace {

// Trivial WidgetDelegate implementation that allows setting return value of
// ShouldAdvanceFocusToTopLevelWidget().
class AdvanceFocusWidgetDelegate : public WidgetDelegate {
 public:
  explicit AdvanceFocusWidgetDelegate(Widget* widget) : widget_(widget) {}
  AdvanceFocusWidgetDelegate(const AdvanceFocusWidgetDelegate&) = delete;
  AdvanceFocusWidgetDelegate& operator=(const AdvanceFocusWidgetDelegate&) =
      delete;
  ~AdvanceFocusWidgetDelegate() override = default;

  // WidgetDelegate:
  Widget* GetWidget() override { return widget_; }
  const Widget* GetWidget() const override { return widget_; }

 private:
  raw_ptr<Widget> widget_;
};

class TestBubbleDialogDelegateView : public BubbleDialogDelegateView {
 public:
  explicit TestBubbleDialogDelegateView(View* anchor)
      : BubbleDialogDelegateView(anchor, BubbleBorder::NONE) {
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kNone));
  }
  TestBubbleDialogDelegateView(const TestBubbleDialogDelegateView&) = delete;
  TestBubbleDialogDelegateView& operator=(const TestBubbleDialogDelegateView&) =
      delete;
  ~TestBubbleDialogDelegateView() override = default;

  static TestBubbleDialogDelegateView* CreateAndShowBubble(View* anchor) {
    TestBubbleDialogDelegateView* bubble =
        new TestBubbleDialogDelegateView(anchor);
    Widget* bubble_widget = BubbleDialogDelegateView::CreateBubble(bubble);
    bubble_widget->SetFocusTraversableParent(
        bubble->anchor_widget()->GetFocusTraversable());
    bubble_widget->SetFocusTraversableParentView(anchor);
    bubble->set_close_on_deactivate(false);
    bubble_widget->Show();
    return bubble;
  }

  // If this is called, the bubble will be forced to use a NativeWidgetAura.
  // If not set, it might get a DesktopNativeWidgetAura depending on the
  // platform and other factors.
  void UseNativeWidgetAura() { use_native_widget_aura_ = true; }

  void OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                Widget* widget) const override {
#if defined(USE_AURA)
    if (use_native_widget_aura_) {
      params->native_widget =
          new test::TestPlatformNativeWidget<NativeWidgetAura>(widget, false,
                                                               nullptr);
    }
#endif  // USE_AURA
  }

 private:
  bool use_native_widget_aura_ = false;
};

}  // namespace

// Verifies focus wrapping happens in the same widget.
TEST_F(FocusManagerTest, AdvanceFocusStaysInWidget) {
  // Add |widget_view| as a child of the Widget.
  View* widget_view = new View;
  widget_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget_view->SetBounds(20, 0, 20, 20);
  GetContentsView()->AddChildView(widget_view);

  // Create a widget with two views, focus the second.
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.child = true;
  params.bounds = gfx::Rect(10, 10, 100, 100);
  params.parent = GetWidget()->GetNativeView();
  auto child_widget = std::make_unique<Widget>();
  std::unique_ptr<AdvanceFocusWidgetDelegate> delegate_owned =
      std::make_unique<AdvanceFocusWidgetDelegate>(child_widget.get());
  params.delegate = delegate_owned.get();
  params.delegate->RegisterDeleteDelegateCallback(
      base::DoNothingWithBoundArgs(std::move(delegate_owned)));
  child_widget->Init(std::move(params));

  View* view1 = new View;
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view1->SetBounds(0, 0, 20, 20);
  View* view2 = new View;
  view2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view2->SetBounds(20, 0, 20, 20);
  child_widget->client_view()->AddChildView(view1);
  child_widget->client_view()->AddChildView(view2);
  child_widget->Show();
  view2->RequestFocus();
  EXPECT_EQ(view2, GetFocusManager()->GetFocusedView());

  // Advance focus backwards, which should focus the first.
  GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(view1, GetFocusManager()->GetFocusedView());

  // Focus forward to |view2|.
  GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(view2, GetFocusManager()->GetFocusedView());

  // And forward again, wrapping back to |view1|.
  GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(view1, GetFocusManager()->GetFocusedView());
}

TEST_F(FocusManagerTest, NavigateIntoAnchoredDialog) {
  // The parent Widget has four focusable views. A child widget dialog has
  // two focusable views, and it's anchored to the 3rd parent view. Ensure
  // that focus traverses into the anchored dialog after the 3rd parent
  // view, and then back to the 4th parent view.

  View* parent1 = new View();
  View* parent2 = new View();
  View* parent3 = new View();
  View* parent4 = new View();

  parent1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  parent2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  parent3->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  parent4->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  GetWidget()->GetRootView()->AddChildView(parent1);
  GetWidget()->GetRootView()->AddChildView(parent2);
  GetWidget()->GetRootView()->AddChildView(parent3);
  GetWidget()->GetRootView()->AddChildView(parent4);

  // Add an unfocusable child view to the dialog anchor view. This is a
  // regression test that makes sure focus is able to navigate past unfocusable
  // children and try to go into the anchored dialog. |kAnchoredDialogKey| was
  // previously not checked if a recursive search to find a focusable child view
  // was attempted (and failed), so the dialog would previously be skipped.
  parent3->AddChildView(new View());

  BubbleDialogDelegateView* bubble_delegate =
      TestBubbleDialogDelegateView::CreateAndShowBubble(parent3);
  Widget* bubble_widget = bubble_delegate->GetWidget();

  View* child1 = new View();
  View* child2 = new View();
  child1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  child2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  bubble_widget->GetRootView()->AddChildView(child1);
  bubble_widget->GetRootView()->AddChildView(child2);

  parent1->RequestFocus();

  // Navigate forwards
  GetWidget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(parent2->HasFocus());
  GetWidget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(parent3->HasFocus());
  GetWidget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(child1->HasFocus());
  bubble_widget->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(child2->HasFocus());
  bubble_widget->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(parent4->HasFocus());

  // Navigate backwards
  GetWidget()->GetFocusManager()->AdvanceFocus(true);
  EXPECT_TRUE(child2->HasFocus());
  bubble_widget->GetFocusManager()->AdvanceFocus(true);
  EXPECT_TRUE(child1->HasFocus());
  bubble_widget->GetFocusManager()->AdvanceFocus(true);
  EXPECT_TRUE(parent3->HasFocus());
}

TEST_F(FocusManagerTest, AnchoredDialogOnContainerView) {
  // The parent Widget has four focusable views, with the middle two views
  // inside of a non-focusable grouping View. A child widget dialog has
  // two focusable views, and it's anchored to the group View. Ensure
  // that focus traverses into the anchored dialog after the 3rd parent
  // view, and then back to the 4th parent view.

  View* parent1 = new View();
  View* parent2 = new View();
  View* parent3 = new View();
  View* parent4 = new View();
  View* parent_group = new View();

  parent1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  parent2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  parent3->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  parent4->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  GetWidget()->GetRootView()->AddChildView(parent1);
  GetWidget()->GetRootView()->AddChildView(parent_group);
  parent_group->AddChildView(parent2);
  parent_group->AddChildView(parent3);
  GetWidget()->GetRootView()->AddChildView(parent4);

  BubbleDialogDelegateView* bubble_delegate =
      TestBubbleDialogDelegateView::CreateAndShowBubble(parent3);
  Widget* bubble_widget = bubble_delegate->GetWidget();

  View* child1 = new View();
  View* child2 = new View();
  child1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  child2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  bubble_widget->GetRootView()->AddChildView(child1);
  bubble_widget->GetRootView()->AddChildView(child2);

  parent1->RequestFocus();

  // Navigate forwards
  GetWidget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(parent2->HasFocus());
  GetWidget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(parent3->HasFocus());
  GetWidget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(child1->HasFocus());
  bubble_widget->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(child2->HasFocus());
  bubble_widget->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(parent4->HasFocus());

  // Navigate backwards
  GetWidget()->GetFocusManager()->AdvanceFocus(true);
  EXPECT_TRUE(child2->HasFocus());
  bubble_widget->GetFocusManager()->AdvanceFocus(true);
  EXPECT_TRUE(child1->HasFocus());
  bubble_widget->GetFocusManager()->AdvanceFocus(true);
  EXPECT_TRUE(parent3->HasFocus());
}

// Checks that focus traverses from a View to a bubble anchored at that View
// when in a pane.
TEST_F(FocusManagerTest, AnchoredDialogInPane) {
  // Set up a focusable view (to which we will anchor our bubble) inside an
  // AccessiblePaneView.
  View* root_view = GetWidget()->GetRootView();
  AccessiblePaneView* pane =
      root_view->AddChildView(std::make_unique<AccessiblePaneView>());
  View* anchor = pane->AddChildView(std::make_unique<View>());
  anchor->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  BubbleDialogDelegateView* bubble =
      TestBubbleDialogDelegateView::CreateAndShowBubble(anchor);

  // We need a focusable view inside our bubble to check that focus traverses
  // in.
  View* bubble_child = bubble->AddChildView(std::make_unique<View>());
  bubble_child->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  // Verify that, when in pane focus mode, focus advances from the anchor view
  // to inside the bubble.
  pane->SetPaneFocus(anchor);
  EXPECT_TRUE(anchor->HasFocus());
  GetWidget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(bubble_child->HasFocus());
}

// Test that a focused view has a visible focus ring.
// This test uses FlexLayout intentionally because it had issues showing focus
// rings.
TEST_F(FocusManagerTest, FocusRing) {
  GetContentsView()->SetLayoutManager(std::make_unique<FlexLayout>());
  View* view = GetContentsView()->AddChildView(
      std::make_unique<StaticSizedView>(gfx::Size(10, 10)));
  GetContentsView()->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  FocusRing::Install(GetContentsView());
  FocusRing::Install(view);

  GetContentsView()->RequestFocus();
  test::RunScheduledLayout(GetWidget());
  EXPECT_TRUE(GetContentsView()->HasFocus());
  EXPECT_TRUE(FocusRing::Get(GetContentsView())->GetVisible());
  EXPECT_FALSE(view->HasFocus());
  EXPECT_FALSE(FocusRing::Get(view)->GetVisible());

  view->RequestFocus();
  test::RunScheduledLayout(GetWidget());
  EXPECT_FALSE(GetContentsView()->HasFocus());
  EXPECT_FALSE(FocusRing::Get(GetContentsView())->GetVisible());
  EXPECT_TRUE(view->HasFocus());
  EXPECT_TRUE(FocusRing::Get(view)->GetVisible());
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
// This test is specifically for the permutation where the main widget is a
// DesktopNativeWidgetAura and the bubble is a NativeWidgetAura. When focus
// moves back from the bubble to the parent widget, ensure that the DNWA's aura
// window is focused.
class DesktopWidgetFocusManagerTest : public FocusManagerTest {
 public:
  DesktopWidgetFocusManagerTest() = default;
  DesktopWidgetFocusManagerTest(const DesktopWidgetFocusManagerTest&) = delete;
  DesktopWidgetFocusManagerTest& operator=(
      const DesktopWidgetFocusManagerTest&) = delete;
  ~DesktopWidgetFocusManagerTest() override = default;

  // FocusManagerTest:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    FocusManagerTest::SetUp();
  }
};

TEST_F(DesktopWidgetFocusManagerTest, AnchoredDialogInDesktopNativeWidgetAura) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();

  View* parent1 = new View();
  View* parent2 = new View();

  parent1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  parent2->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  widget->GetRootView()->AddChildView(parent1);
  widget->GetRootView()->AddChildView(parent2);

  TestBubbleDialogDelegateView* bubble_delegate =
      TestBubbleDialogDelegateView::CreateAndShowBubble(parent2);
  Widget* bubble_widget = bubble_delegate->GetWidget();
  bubble_delegate->UseNativeWidgetAura();

  View* child = new View();
  child->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  bubble_widget->GetRootView()->AddChildView(child);

  // In order to pass the accessibility paint checks, focusable views must have
  // a valid role.
  parent1->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  parent2->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  child->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  // In order to pass the accessibility paint checks, focusable views must have
  // a non-empty accessible name, or have their name set to explicitly empty.
  parent1->GetViewAccessibility().SetName(u"Parent 1",
                                          ax::mojom::NameFrom::kAttribute);
  parent2->GetViewAccessibility().SetName(
      u"", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  child->GetViewAccessibility().SetName("uChild",
                                        ax::mojom::NameFrom::kAttribute);

  widget->Activate();
  parent1->RequestFocus();
  base::RunLoop().RunUntilIdle();

  // Initially the outer widget's window is focused.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(widget->GetNativeView());
  ASSERT_EQ(widget->GetNativeView(), focus_client->GetFocusedWindow());

  // Navigate forwards
  widget->GetFocusManager()->AdvanceFocus(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(parent2->HasFocus());
  widget->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(child->HasFocus());

  // Now the bubble widget's window is focused.
  ASSERT_NE(widget->GetNativeView(), focus_client->GetFocusedWindow());
  ASSERT_EQ(bubble_widget->GetNativeView(), focus_client->GetFocusedWindow());

  // Navigate backwards
  bubble_widget->GetFocusManager()->AdvanceFocus(true);
  EXPECT_TRUE(parent2->HasFocus());

  // Finally, the outer widget's window should be focused again.
  ASSERT_EQ(widget->GetNativeView(), focus_client->GetFocusedWindow());
}
#endif

#if defined(USE_AURA)
class RedirectToParentFocusManagerTest : public FocusManagerTest {
 public:
  RedirectToParentFocusManagerTest() = default;
  RedirectToParentFocusManagerTest(const RedirectToParentFocusManagerTest&) =
      delete;
  RedirectToParentFocusManagerTest& operator=(
      const RedirectToParentFocusManagerTest&) = delete;
  ~RedirectToParentFocusManagerTest() override = default;

  // FocusManagerTest:
  void SetUp() override {
    FocusManagerTest::SetUp();

    View* anchor =
        GetWidget()->GetRootView()->AddChildView(std::make_unique<View>());
    anchor->SetFocusBehavior(View::FocusBehavior::ALWAYS);

    bubble_ = TestBubbleDialogDelegateView::CreateAndShowBubble(anchor);
    Widget* bubble_widget = bubble_->GetWidget();

    parent_focus_manager_ = anchor->GetFocusManager();
    bubble_focus_manager_ = bubble_widget->GetFocusManager();
  }

  void TearDown() override {
    parent_focus_manager_ = nullptr;
    bubble_focus_manager_ = nullptr;
    bubble_ = nullptr;
    FocusManagerFactory::Install(nullptr);
    FocusManagerTest::TearDown();
  }

 protected:
  raw_ptr<FocusManager> parent_focus_manager_ = nullptr;
  raw_ptr<FocusManager> bubble_focus_manager_ = nullptr;
  raw_ptr<BubbleDialogDelegateView> bubble_ = nullptr;
};

// Test that when an accelerator is sent to a bubble that isn't registered,
// the bubble's parent handles it instead.
TEST_F(RedirectToParentFocusManagerTest, ParentHandlesAcceleratorFromBubble) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  ui::TestAcceleratorTarget parent_return_target(true);
  Widget* bubble_widget = bubble_->GetWidget();

  EXPECT_EQ(0, parent_return_target.accelerator_count());
  parent_focus_manager_->RegisterAccelerator(
      return_accelerator, ui::AcceleratorManager::kNormalPriority,
      &parent_return_target);

  EXPECT_TRUE(
      !bubble_focus_manager_->IsAcceleratorRegistered(return_accelerator));

  // The bubble should be closed after parent processed accelerator only if
  // close_on_deactivate is true.
  bubble_->set_close_on_deactivate(false);
  // Accelerator was processed by the parent.
  EXPECT_TRUE(bubble_focus_manager_->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(parent_return_target.accelerator_count(), 1);
  EXPECT_FALSE(bubble_widget->IsClosed());

  // Reset focus to the bubble widget. Focus was set to the the main widget
  // to process accelerator.
  bubble_focus_manager_->SetFocusedView(bubble_widget->GetRootView());

  bubble_->set_close_on_deactivate(true);
  EXPECT_TRUE(bubble_focus_manager_->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(parent_return_target.accelerator_count(), 2);
  EXPECT_TRUE(bubble_widget->IsClosed());
}

// Test that when an accelerator is sent to a bubble that is registered on both
// it and its parent, the bubble handles it.
TEST_F(RedirectToParentFocusManagerTest, BubbleHandlesRegisteredAccelerators) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  ui::TestAcceleratorTarget parent_return_target(true);
  ui::TestAcceleratorTarget bubble_return_target(true);
  Widget* bubble_widget = bubble_->GetWidget();

  EXPECT_EQ(0, bubble_return_target.accelerator_count());
  EXPECT_EQ(0, parent_return_target.accelerator_count());

  bubble_focus_manager_->RegisterAccelerator(
      return_accelerator, ui::AcceleratorManager::kNormalPriority,
      &bubble_return_target);
  parent_focus_manager_->RegisterAccelerator(
      return_accelerator, ui::AcceleratorManager::kNormalPriority,
      &parent_return_target);

  // The bubble shouldn't be closed after it processed accelerator without
  // passing it to the parent.
  bubble_->set_close_on_deactivate(true);
  // Accelerator was processed by the bubble and not by the parent.
  EXPECT_TRUE(bubble_focus_manager_->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, bubble_return_target.accelerator_count());
  EXPECT_EQ(0, parent_return_target.accelerator_count());
  EXPECT_FALSE(bubble_widget->IsClosed());
}

// Test that when an accelerator is sent to a bubble that isn't registered
// for either the bubble or the bubble's parent, the bubble isn't closed.
TEST_F(RedirectToParentFocusManagerTest, NotProcessedAccelerator) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  Widget* bubble_widget = bubble_->GetWidget();

  EXPECT_TRUE(
      !bubble_focus_manager_->IsAcceleratorRegistered(return_accelerator));
  EXPECT_TRUE(
      !parent_focus_manager_->IsAcceleratorRegistered(return_accelerator));

  // The bubble shouldn't be closed if the accelerator was passed to the parent
  // but the parent didn't process it.
  bubble_->set_close_on_deactivate(true);
  EXPECT_FALSE(bubble_focus_manager_->ProcessAccelerator(return_accelerator));
  EXPECT_FALSE(bubble_widget->IsClosed());
}

#endif

}  // namespace views
