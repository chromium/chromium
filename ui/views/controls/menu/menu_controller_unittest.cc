// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/menu/menu_host_root_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_utils.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/views/controls/menu/menu_pre_target_handler.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_platform.h"
#if BUILDFLAG(OZONE_PLATFORM_X11)
#define USE_OZONE_PLATFORM_X11
#endif
#endif

#if defined(USE_OZONE_PLATFORM_X11)
#include "ui/events/test/events_test_utils_x11.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace views::test {
namespace {

using ::ui::mojom::DragOperation;

bool ShouldIgnoreScreenBoundsForMenus() {
#if BUILDFLAG(IS_OZONE)
  // Some platforms, such as Wayland, disallow client applications to manipulate
  // global screen coordinates, requiring menus to be positioned relative to
  // their parent windows. See comment in ozone_platform_wayland.cc.
  return !ui::OzonePlatform::GetInstance()
              ->GetPlatformProperties()
              .supports_global_screen_coordinates;
#else
  return false;
#endif
}

// Test implementation of MenuControllerDelegate that only reports the values
// called of OnMenuClosed.
class TestMenuControllerDelegate : public internal::MenuControllerDelegate {
 public:
  TestMenuControllerDelegate();

  TestMenuControllerDelegate(const TestMenuControllerDelegate&) = delete;
  TestMenuControllerDelegate& operator=(const TestMenuControllerDelegate&) =
      delete;

  ~TestMenuControllerDelegate() override = default;

  int on_menu_closed_called() { return on_menu_closed_called_; }

  NotifyType on_menu_closed_notify_type() {
    return on_menu_closed_notify_type_;
  }

  MenuItemView* on_menu_closed_menu() { return on_menu_closed_menu_; }

  int on_menu_closed_mouse_event_flags() {
    return on_menu_closed_mouse_event_flags_;
  }

  // On a subsequent call to OnMenuClosed |controller| will be deleted.
  void set_on_menu_closed_callback(base::RepeatingClosure callback) {
    on_menu_closed_callback_ = std::move(callback);
  }

  // internal::MenuControllerDelegate:
  void OnMenuClosed(NotifyType type,
                    MenuItemView* menu,
                    int mouse_event_flags) override;
  void SiblingMenuCreated(MenuItemView* menu) override;

 private:
  // Number of times OnMenuClosed has been called.
  int on_menu_closed_called_ = 0;

  // The values passed on the last call of OnMenuClosed.
  NotifyType on_menu_closed_notify_type_ = NOTIFY_DELEGATE;
  raw_ptr<MenuItemView> on_menu_closed_menu_ = nullptr;
  int on_menu_closed_mouse_event_flags_ = 0;

  // Optional callback triggered during OnMenuClosed
  base::RepeatingClosure on_menu_closed_callback_;
};

TestMenuControllerDelegate::TestMenuControllerDelegate() = default;

void TestMenuControllerDelegate::OnMenuClosed(NotifyType type,
                                              MenuItemView* menu,
                                              int mouse_event_flags) {
  on_menu_closed_called_++;
  on_menu_closed_notify_type_ = type;
  on_menu_closed_menu_ = menu;
  on_menu_closed_mouse_event_flags_ = mouse_event_flags;
  if (!on_menu_closed_callback_.is_null())
    on_menu_closed_callback_.Run();
}

void TestMenuControllerDelegate::SiblingMenuCreated(MenuItemView* menu) {}

class SubmenuViewShown : public SubmenuView {
 public:
  using SubmenuView::SubmenuView;

  SubmenuViewShown(const SubmenuViewShown&) = delete;
  SubmenuViewShown& operator=(const SubmenuViewShown&) = delete;

  ~SubmenuViewShown() override = default;
  bool IsShowing() const override { return true; }
};

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() = default;

  TestEventHandler(const TestEventHandler&) = delete;
  TestEventHandler& operator=(const TestEventHandler&) = delete;

  void OnTouchEvent(ui::TouchEvent* event) override {
    switch (event->type()) {
      case ui::ET_TOUCH_PRESSED:
        outstanding_touches_++;
        break;
      case ui::ET_TOUCH_RELEASED:
      case ui::ET_TOUCH_CANCELLED:
        outstanding_touches_--;
        break;
      default:
        break;
    }
  }

  int outstanding_touches() const { return outstanding_touches_; }

 private:
  int outstanding_touches_ = 0;
};

// A test widget that counts gesture events.
class GestureTestWidget : public Widget {
 public:
  GestureTestWidget() = default;

  GestureTestWidget(const GestureTestWidget&) = delete;
  GestureTestWidget& operator=(const GestureTestWidget&) = delete;

  void OnGestureEvent(ui::GestureEvent* event) override { ++gesture_count_; }

  int gesture_count() const { return gesture_count_; }

 private:
  int gesture_count_ = 0;
};

#if defined(USE_AURA)
// A DragDropClient which does not trigger a nested run loop. Instead a
// callback is triggered during StartDragAndDrop in order to allow testing.
class TestDragDropClient : public aura::client::DragDropClient {
 public:
  explicit TestDragDropClient(base::RepeatingClosure callback)
      : start_drag_and_drop_callback_(std::move(callback)) {}

  TestDragDropClient(const TestDragDropClient&) = delete;
  TestDragDropClient& operator=(const TestDragDropClient&) = delete;

  ~TestDragDropClient() override = default;

  // aura::client::DragDropClient:
  DragOperation StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                                 aura::Window* root_window,
                                 aura::Window* source_window,
                                 const gfx::Point& screen_location,
                                 int allowed_operations,
                                 ui::mojom::DragEventSource source) override;
#if BUILDFLAG(IS_LINUX)
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override {}
#endif
  void DragCancel() override;
  bool IsDragDropInProgress() override;

  void AddObserver(aura::client::DragDropClientObserver* observer) override {}
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override {
  }

 private:
  base::RepeatingClosure start_drag_and_drop_callback_;
  bool drag_in_progress_ = false;
};

DragOperation TestDragDropClient::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  drag_in_progress_ = true;
  start_drag_and_drop_callback_.Run();
  return DragOperation::kNone;
}

void TestDragDropClient::DragCancel() {
  drag_in_progress_ = false;
}
bool TestDragDropClient::IsDragDropInProgress() {
  return drag_in_progress_;
}

#endif  // defined(USE_AURA)

// View which cancels the menu it belongs to on mouse press.
class CancelMenuOnMousePressView : public View {
 public:
  explicit CancelMenuOnMousePressView(MenuController* controller)
      : controller_(controller) {}

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    controller_->Cancel(MenuController::ExitType::kAll);
    return true;
  }

  // This is needed to prevent the view from being "squashed" to zero height
  // when the menu which owns it is shown. In such state the logic which
  // determines if the menu contains the mouse press location doesn't work.
  gfx::Size CalculatePreferredSize() const override { return size(); }

 private:
  raw_ptr<MenuController> controller_;
};

}  // namespace

class TestMenuItemViewShown : public MenuItemView {
 public:
  explicit TestMenuItemViewShown(MenuDelegate* delegate)
      : MenuItemView(delegate) {
    submenu_ = new SubmenuViewShown(this);
  }

  TestMenuItemViewShown(const TestMenuItemViewShown&) = delete;
  TestMenuItemViewShown& operator=(const TestMenuItemViewShown&) = delete;

  ~TestMenuItemViewShown() override = default;

  void SetController(MenuController* controller) { set_controller(controller); }

  void AddEmptyMenusForTest() { AddEmptyMenus(); }

  void SetActualMenuPosition(MenuItemView::MenuPosition position) {
    set_actual_menu_position(position);
  }
  MenuItemView::MenuPosition ActualMenuPosition() {
    return actual_menu_position();
  }
};

class TestMenuItemViewNotShown : public MenuItemView {
 public:
  explicit TestMenuItemViewNotShown(MenuDelegate* delegate)
      : MenuItemView(delegate) {
    submenu_ = new SubmenuView(this);
  }

  TestMenuItemViewNotShown(const TestMenuItemViewNotShown&) = delete;
  TestMenuItemViewNotShown& operator=(const TestMenuItemViewNotShown&) = delete;

  ~TestMenuItemViewNotShown() override = default;

  void SetController(MenuController* controller) { set_controller(controller); }
};

struct MenuBoundsOptions {
 public:
  gfx::Rect anchor_bounds = gfx::Rect(500, 500, 10, 10);
  gfx::Rect monitor_bounds = gfx::Rect(0, 0, 1000, 1000);
  gfx::Size menu_size = gfx::Size(100, 100);
  MenuAnchorPosition menu_anchor = MenuAnchorPosition::kTopLeft;
  MenuItemView::MenuPosition menu_position =
      MenuItemView::MenuPosition::kBestFit;
};

class MenuControllerTest : public ViewsTestBase,
                           public testing::WithParamInterface<bool> {
 public:
  MenuControllerTest() = default;

  MenuControllerTest(const MenuControllerTest&) = delete;
  MenuControllerTest& operator=(const MenuControllerTest&) = delete;

  ~MenuControllerTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      // Setup right to left environment if necessary.
      if (GetParam())
        base::i18n::SetRTLForTesting(true);
    }

    auto test_views_delegate = std::make_unique<ReleaseRefTestViewsDelegate>();
    test_views_delegate_ = test_views_delegate.get();
    // ViewsTestBase takes ownership, destroying during Teardown.
    set_views_delegate(std::move(test_views_delegate));
    ViewsTestBase::SetUp();
    Init();
    ASSERT_TRUE(base::CurrentUIThread::IsSet());
  }

  void TearDown() override {
    owner_->CloseNow();
    DestroyMenuController();
    ViewsTestBase::TearDown();
    base::i18n::SetRTLForTesting(false);
  }

  void ReleaseTouchId(int id) { event_generator_->ReleaseTouchId(id); }

  void PressKey(ui::KeyboardCode key_code) {
    event_generator_->PressKey(key_code, 0);
  }

  void DispatchKey(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::EventType::ET_KEY_PRESSED, key_code, 0);
    menu_controller_->OnWillDispatchKeyEvent(&event);
  }

  gfx::Rect CalculateMenuBounds(const MenuBoundsOptions& options) {
    SetUpMenuControllerForCalculateBounds(options);
    bool is_leading;
    ui::OwnedWindowAnchor anchor;
    return menu_controller_->CalculateMenuBounds(menu_item_.get(), true,
                                                 &is_leading, &anchor);
  }

  gfx::Rect CalculateBubbleMenuBounds(const MenuBoundsOptions& options,
                                      MenuItemView* menu_item) {
    SetUpMenuControllerForCalculateBounds(options);
    bool is_leading;
    ui::OwnedWindowAnchor anchor;
    return menu_controller_->CalculateBubbleMenuBounds(menu_item, true,
                                                       &is_leading, &anchor);
  }

  gfx::Rect CalculateBubbleMenuBounds(const MenuBoundsOptions& options) {
    return CalculateBubbleMenuBounds(options, menu_item_.get());
  }

  gfx::Rect CalculateExpectedMenuAnchorRect(MenuItemView* menu_item,
                                            const gfx::Rect& item_bounds) {
    if (menu_item->GetParentMenuItem()) {
      gfx::Rect anchor_rect = item_bounds;
      anchor_rect.set_size({1, 1});
      const MenuConfig& menu_config = MenuConfig::instance();
      const int submenu_horizontal_inset = menu_config.submenu_horizontal_inset;

      const int left_of_parent = menu_item->GetBoundsInScreen().x() -
                                 item_bounds.width() + submenu_horizontal_inset;

      // TODO(1163646): handle RTL layout.
      anchor_rect.set_x(left_of_parent + item_bounds.width());
      anchor_rect.set_width(item_bounds.x() - anchor_rect.x());
      return anchor_rect;
    }
    return menu_item->bounds();
  }

  void MenuChildrenChanged(MenuItemView* item) {
    menu_controller_->MenuChildrenChanged(item);
  }

  static MenuAnchorPosition AdjustAnchorPositionForRtl(
      MenuAnchorPosition position) {
    return MenuController::AdjustAnchorPositionForRtl(position);
  }

#if defined(USE_AURA)
  // Verifies that a non-nested menu fully closes when receiving an escape key.
  void TestAsyncEscapeKey() {
    ui::KeyEvent event(ui::EventType::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0);
    menu_controller_->OnWillDispatchKeyEvent(&event);
  }

  // Verifies that an open menu receives a cancel event, and closes.
  void TestCancelEvent() {
    EXPECT_EQ(MenuController::ExitType::kNone, menu_controller_->exit_type());
    ui::CancelModeEvent cancel_event;
    event_generator_->Dispatch(&cancel_event);
    EXPECT_EQ(MenuController::ExitType::kAll, menu_controller_->exit_type());
  }
#endif  // defined(USE_AURA)

  // Verifies the state of the |menu_controller_| before destroying it.
  void VerifyDragCompleteThenDestroy() {
    EXPECT_FALSE(menu_controller()->drag_in_progress());
    EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
    DestroyMenuController();
  }

  // Setups |menu_controller_delegate_| to be destroyed when OnMenuClosed is
  // called.
  void TestDragCompleteThenDestroyOnMenuClosed() {
    menu_controller_delegate_->set_on_menu_closed_callback(
        base::BindRepeating(&MenuControllerTest::VerifyDragCompleteThenDestroy,
                            base::Unretained(this)));
  }

  // Tests destroying the active |menu_controller_| and replacing it with a new
  // active instance.
  void TestMenuControllerReplacementDuringDrag() {
    DestroyMenuController();
    menu_item()->GetSubmenu()->Close();
    const bool for_drop = false;
    menu_controller_ =
        new MenuController(for_drop, menu_controller_delegate_.get());
    menu_controller_->owner_ = owner_.get();
    menu_controller_->showing_ = true;
  }

  // Tests that the menu does not destroy itself when canceled during a drag.
  void TestCancelAllDuringDrag() {
    menu_controller_->Cancel(MenuController::ExitType::kAll);
    EXPECT_EQ(0, menu_controller_delegate_->on_menu_closed_called());
  }

  // Tests that destroying the menu during ViewsDelegate::ReleaseRef does not
  // cause a crash.
  void TestDestroyedDuringViewsRelease() {
    // |test_views_delegate_| is owned by views::ViewsTestBase and not deleted
    // until TearDown. MenuControllerTest outlives it.
    test_views_delegate_->set_release_ref_callback(base::BindRepeating(
        &MenuControllerTest::DestroyMenuController, base::Unretained(this)));
    menu_controller_->ExitMenu();
  }

  void TestMenuFitsOnScreen(MenuAnchorPosition menu_anchor_position,
                            const gfx::Rect& monitor_bounds) {
    SCOPED_TRACE(base::StringPrintf(
        "MenuAnchorPosition: %d, monitor_bounds: @%s\n", menu_anchor_position,
        monitor_bounds.ToString().c_str()));
    MenuBoundsOptions options;
    options.menu_anchor = menu_anchor_position;
    options.monitor_bounds = monitor_bounds;
    const gfx::Point monitor_center = monitor_bounds.CenterPoint();

    // Simulate a bottom shelf with a tall menu.
    const int button_size = 50;
    options.anchor_bounds =
        gfx::Rect(monitor_center.x(), monitor_bounds.bottom() - button_size,
                  button_size, button_size);
    gfx::Rect final_bounds = CalculateBubbleMenuBounds(options);

    // Adjust the final bounds to not include the shadow and border.
    const gfx::Insets border_and_shadow_insets =
        GetBorderAndShadowInsets(/*is_submenu=*/false);
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu will show on screen.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate a left shelf with a tall menu.
    options.anchor_bounds = gfx::Rect(monitor_bounds.x(), monitor_center.y(),
                                      button_size, button_size);
    final_bounds = CalculateBubbleMenuBounds(options);

    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu will show on screen.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate right shelf with a tall menu.
    options.anchor_bounds =
        gfx::Rect(monitor_bounds.right() - button_size, monitor_center.y(),
                  button_size, button_size);
    final_bounds = CalculateBubbleMenuBounds(options);

    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu will show on screen.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));
  }

  void TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition menu_anchor_position,
                                       const gfx::Rect& monitor_bounds) {
    SCOPED_TRACE(base::StringPrintf(
        "MenuAnchorPosition: %d, monitor_bounds: @%s\n", menu_anchor_position,
        monitor_bounds.ToString().c_str()));
    MenuBoundsOptions options;
    options.menu_anchor = menu_anchor_position;
    options.monitor_bounds = monitor_bounds;
    const gfx::Size anchor_size(0, 0);

    // Simulate a click on the top left corner.
    options.anchor_bounds = gfx::Rect(monitor_bounds.origin(), anchor_size);
    gfx::Rect final_bounds = CalculateBubbleMenuBounds(options);

    // Adjust the final bounds to not include the shadow and border.
    const gfx::Insets border_and_shadow_insets =
        GetBorderAndShadowInsets(/*is_submenu=*/false);
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu is within the monitor bounds.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate a click on the bottom left corner.
    options.anchor_bounds =
        gfx::Rect(monitor_bounds.bottom_left(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu is within the monitor bounds.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate a click on the top right corner.
    options.anchor_bounds = gfx::Rect(monitor_bounds.top_right(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu is within the monitor bounds.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate a click on the bottom right corner.
    options.anchor_bounds =
        gfx::Rect(monitor_bounds.bottom_right(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu is within the monitor bounds.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));
  }

  void TestMenuFitsOnSmallScreen(MenuAnchorPosition menu_anchor_position,
                                 const gfx::Rect& monitor_bounds) {
    SCOPED_TRACE(base::StringPrintf(
        "MenuAnchorPosition: %d, monitor_bounds: @%s\n", menu_anchor_position,
        monitor_bounds.ToString().c_str()));
    MenuBoundsOptions options;
    options.menu_anchor = menu_anchor_position;
    options.monitor_bounds = monitor_bounds;
    options.menu_size = monitor_bounds.size();
    options.menu_size.Enlarge(100, 100);
    const gfx::Size anchor_size(0, 0);

    // Adjust the final bounds to not include the shadow and border.
    const gfx::Insets border_and_shadow_insets =
        GetBorderAndShadowInsets(/*is_submenu=*/false);

    options.anchor_bounds = gfx::Rect(monitor_bounds.origin(), anchor_size);
    gfx::Rect final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();

    options.anchor_bounds =
        gfx::Rect(monitor_bounds.bottom_left(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();

    options.anchor_bounds =
        gfx::Rect(monitor_bounds.bottom_right(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();

    options.anchor_bounds = gfx::Rect(monitor_bounds.top_right(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();

    options.anchor_bounds =
        gfx::Rect(monitor_bounds.CenterPoint(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();
  }

  void TestSubmenuFitsOnScreen(MenuItemView* item,
                               const gfx::Rect& monitor_bounds,
                               const gfx::Rect& parent_bounds,
                               MenuAnchorPosition menu_anchor) {
    MenuBoundsOptions options;
    options.menu_anchor = menu_anchor;
    options.monitor_bounds = monitor_bounds;

    // Adjust the final bounds to not include the shadow and border.
    const gfx::Insets border_and_shadow_insets =
        GetBorderAndShadowInsets(/*is_submenu=*/true);

    MenuItemView* parent_item = item->GetParentMenuItem();
    SubmenuView* sub_menu = parent_item->GetSubmenu();

    parent_item->SetBoundsRect(parent_bounds);
    MenuHost::InitParams params;
    params.parent = owner();
    params.bounds = parent_item->bounds();
    params.do_capture = false;
    sub_menu->ShowAt(params);
    gfx::Rect final_bounds = CalculateBubbleMenuBounds(options, item);
    final_bounds.Inset(border_and_shadow_insets);
    sub_menu->Close();

    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();
  }

 protected:
  void SetPendingStateItem(MenuItemView* item) {
    menu_controller_->pending_state_.item = item;
    menu_controller_->pending_state_.submenu_open = true;
  }

  void SetState(MenuItemView* item) {
    menu_controller_->state_.item = item;
    menu_controller_->state_.submenu_open = true;
  }

  void ResetSelection() {
    menu_controller_->SetSelection(
        nullptr, MenuController::SELECTION_EXIT |
                     MenuController::SELECTION_UPDATE_IMMEDIATELY);
  }

  void IncrementSelection() {
    menu_controller_->IncrementSelection(
        MenuController::INCREMENT_SELECTION_DOWN);
  }

  void DecrementSelection() {
    menu_controller_->IncrementSelection(
        MenuController::INCREMENT_SELECTION_UP);
  }

  void DestroyMenuControllerOnMenuClosed(TestMenuControllerDelegate* delegate) {
    // Unretained() is safe here as the test should outlive the delegate. If not
    // we want to know.
    delegate->set_on_menu_closed_callback(base::BindRepeating(
        &MenuControllerTest::DestroyMenuController, base::Unretained(this)));
  }

  MenuItemView* FindInitialSelectableMenuItemDown(MenuItemView* parent) {
    return menu_controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_DOWN);
  }

  MenuItemView* FindInitialSelectableMenuItemUp(MenuItemView* parent) {
    return menu_controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_UP);
  }

  internal::MenuControllerDelegate* GetCurrentDelegate() {
    return menu_controller_->delegate_;
  }

  bool IsShowing() { return menu_controller_->showing_; }

  MenuHost* GetMenuHost(SubmenuView* submenu) { return submenu->host_; }

  MenuHostRootView* CreateMenuHostRootView(MenuHost* host) {
    return static_cast<MenuHostRootView*>(host->CreateRootView());
  }

  void MenuHostOnDragWillStart(MenuHost* host) { host->OnDragWillStart(); }

  void MenuHostOnDragComplete(MenuHost* host) { host->OnDragComplete(); }

  void SelectByChar(char16_t character) {
    menu_controller_->SelectByChar(character);
  }

  void SetDropMenuItem(MenuItemView* target,
                       MenuDelegate::DropPosition position) {
    menu_controller_->SetDropMenuItem(target, position);
  }

  void SetComboboxType(MenuController::ComboboxType combobox_type) {
    menu_controller_->set_combobox_type(combobox_type);
  }

  void SetSelectionOnPointerDown(SubmenuView* source,
                                 const ui::LocatedEvent* event) {
    menu_controller_->SetSelectionOnPointerDown(source, event);
  }

  // Note that coordinates of events passed to MenuController must be in that of
  // the MenuScrollViewContainer.
  void ProcessGestureEvent(SubmenuView* source, ui::GestureEvent& event) {
    menu_controller_->OnGestureEvent(source, &event);
  }

  void ProcessMousePressed(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMousePressed(source, event);
  }

  void ProcessMouseDragged(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMouseDragged(source, event);
  }

  void ProcessMouseMoved(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMouseMoved(source, event);
  }

  void ProcessMouseReleased(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMouseReleased(source, event);
  }

  void Accept(MenuItemView* item, int event_flags) {
    menu_controller_->Accept(item, event_flags);
    views::test::WaitForMenuClosureAnimation();
  }

  // Causes the |menu_controller_| to begin dragging. Use TestDragDropClient to
  // avoid nesting message loops.
  void StartDrag() {
    const gfx::Point location;
    menu_controller_->state_.item = menu_item()->GetSubmenu()->GetMenuItemAt(0);
    menu_controller_->StartDrag(
        menu_item()->GetSubmenu()->GetMenuItemAt(0)->CreateSubmenu(), location);
  }

  void SetUpMenuControllerForCalculateBounds(const MenuBoundsOptions& options) {
    menu_controller_->state_.anchor = options.menu_anchor;
    menu_controller_->state_.initial_bounds = options.anchor_bounds;
    menu_controller_->state_.monitor_bounds = options.monitor_bounds;
    menu_item_->SetActualMenuPosition(options.menu_position);
    menu_item_->GetSubmenu()->GetScrollViewContainer()->SetPreferredSize(
        options.menu_size);
  }

  GestureTestWidget* owner() { return owner_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  TestMenuItemViewShown* menu_item() { return menu_item_.get(); }
  TestMenuDelegate* menu_delegate() { return menu_delegate_.get(); }
  TestMenuControllerDelegate* menu_controller_delegate() {
    return menu_controller_delegate_.get();
  }
  MenuController* menu_controller() { return menu_controller_; }
  const MenuItemView* pending_state_item() const {
    return menu_controller_->pending_state_.item;
  }
  MenuController::ExitType menu_exit_type() const {
    return menu_controller_->exit_type_;
  }

  // Adds a menu item having buttons as children and returns it. If
  // `single_child` is true, the hosting menu item has only one child button.
  MenuItemView* AddButtonMenuItems(bool single_child) {
    menu_item()->SetBounds(0, 0, 200, 300);
    MenuItemView* item_view = menu_item()->AppendMenuItem(5, u"Five");
    const size_t children_count = single_child ? 1 : 3;
    for (size_t i = 0; i < children_count; ++i) {
      LabelButton* button =
          new LabelButton(Button::PressedCallback(), u"Label");
      // This is an in-menu button. Hence it must be always focusable.
      button->SetFocusBehavior(View::FocusBehavior::ALWAYS);
      item_view->AddChildView(button);
    }
    MenuHost::InitParams params;
    params.parent = owner();
    params.bounds = menu_item()->bounds();
    params.do_capture = false;
    menu_item()->GetSubmenu()->ShowAt(params);
    return item_view;
  }

  void DestroyMenuItem() { menu_item_.reset(); }

  Button* GetHotButton() { return menu_controller_->hot_button_; }

  void SetHotTrackedButton(Button* hot_button) {
    menu_controller_->SetHotTrackedButton(hot_button);
  }

  void ExitMenuRun() {
    menu_controller_->SetExitType(MenuController::ExitType::kOutermost);
    menu_controller_->ExitTopMostMenu();
  }

  void DestroyMenuController() {
    if (!menu_controller_)
      return;

    if (!owner_->IsClosed())
      owner_->RemoveObserver(menu_controller_);

    menu_controller_->showing_ = false;
    menu_controller_->owner_ = nullptr;
    delete menu_controller_;
    menu_controller_ = nullptr;
  }

  int CountOwnerOnGestureEvent() const { return owner_->gesture_count(); }

  bool SelectionWraps() {
    return MenuConfig::instance().arrow_key_selection_wraps;
  }

  void OpenMenu(MenuItemView* parent) {
    menu_controller_->OpenMenuImpl(parent, true);
  }

  gfx::Insets GetBorderAndShadowInsets(bool is_submenu) {
    const MenuConfig& menu_config = MenuConfig::instance();
    int elevation = menu_config.touchable_menu_shadow_elevation;
    BubbleBorder::Shadow shadow_type = BubbleBorder::STANDARD_SHADOW;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Increase the submenu shadow elevation and change the shadow style to
    // ChromeOS system UI shadow style when using Ash System UI layout.
    if (menu_controller_->use_ash_system_ui_layout()) {
      if (is_submenu)
        elevation = menu_config.touchable_submenu_shadow_elevation;

      shadow_type = BubbleBorder::CHROMEOS_SYSTEM_UI_SHADOW;
    }
#endif
    return BubbleBorder::GetBorderAndShadowInsets(elevation, shadow_type);
  }

 private:
  void Init() {
    owner_ = std::make_unique<GestureTestWidget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    owner_->Init(std::move(params));
    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(owner()));
    owner_->Show();

    SetupMenuItem();
    SetupMenuController();
  }

  void SetupMenuItem() {
    menu_delegate_ = std::make_unique<TestMenuDelegate>();
    menu_item_ = std::make_unique<TestMenuItemViewShown>(menu_delegate_.get());
    menu_item_->AppendMenuItem(1, u"One");
    menu_item_->AppendMenuItem(2, u"Two");
    menu_item_->AppendMenuItem(3, u"Three");
    menu_item_->AppendMenuItem(4, u"Four");
  }

  void SetupMenuController() {
    menu_controller_delegate_ = std::make_unique<TestMenuControllerDelegate>();
    const bool for_drop = false;
    menu_controller_ =
        new MenuController(for_drop, menu_controller_delegate_.get());
    menu_controller_->owner_ = owner_.get();
    menu_controller_->showing_ = true;
    menu_controller_->SetSelection(
        menu_item_.get(), MenuController::SELECTION_UPDATE_IMMEDIATELY);
    menu_item_->SetController(menu_controller_);
  }

  // Not owned.
  raw_ptr<ReleaseRefTestViewsDelegate> test_views_delegate_ = nullptr;

  std::unique_ptr<GestureTestWidget> owner_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<TestMenuItemViewShown> menu_item_;
  std::unique_ptr<TestMenuControllerDelegate> menu_controller_delegate_;
  std::unique_ptr<TestMenuDelegate> menu_delegate_;
  raw_ptr<MenuController> menu_controller_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All, MenuControllerTest, testing::Bool());

#if defined(USE_AURA)
// Tests that an event targeter which blocks events will be honored by the menu
// event dispatcher.
TEST_F(MenuControllerTest, EventTargeter) {
  {
    // With the aura::NullWindowTargeter instantiated and assigned we expect
    // the menu to not handle the key event.
    aura::ScopedWindowTargeter scoped_targeter(
        GetRootWindow(owner()), std::make_unique<aura::NullWindowTargeter>());
    PressKey(ui::VKEY_ESCAPE);
    EXPECT_EQ(MenuController::ExitType::kNone, menu_exit_type());
  }
  // Now that the targeter has been destroyed, expect to exit the menu
  // normally when hitting escape.
  TestAsyncEscapeKey();
  EXPECT_EQ(MenuController::ExitType::kAll, menu_exit_type());
}
#endif  // defined(USE_AURA)

#if defined(USE_OZONE_PLATFORM_X11)
// Tests that touch event ids are released correctly. See crbug.com/439051 for
// details. When the ids aren't managed correctly, we get stuck down touches.
TEST_F(MenuControllerTest, TouchIdsReleasedCorrectly) {
  // Run this test only for X11.
  if (ui::OzonePlatform::GetPlatformNameForTest() != "x11")
    GTEST_SKIP();

  TestEventHandler test_event_handler;
  GetRootWindow(owner())->AddPreTargetHandler(&test_event_handler);

  std::vector<int> devices;
  devices.push_back(1);
  ui::SetUpTouchDevicesForTest(devices);

  event_generator()->PressTouchId(0);
  event_generator()->PressTouchId(1);
  event_generator()->ReleaseTouchId(0);

  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  MenuControllerTest::ReleaseTouchId(1);
  TestAsyncEscapeKey();

  EXPECT_EQ(MenuController::ExitType::kAll, menu_exit_type());
  EXPECT_EQ(0, test_event_handler.outstanding_touches());

  GetRootWindow(owner())->RemovePreTargetHandler(&test_event_handler);
}
#endif  // defined(USE_OZONE_PLATFORM_X11)

// Tests that initial selected menu items are correct when items are enabled or
// disabled.
TEST_F(MenuControllerTest, InitialSelectedItem) {
  // Leave items "Two", "Three", and "Four" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  // The first selectable item should be item "Two".
  MenuItemView* first_selectable =
      FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  // The last selectable item should be item "Four".
  MenuItemView* last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(4, last_selectable->GetCommand());

  // Leave items "One" and "Two" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "Two".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // Leave only a single item "One" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "One".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(1, last_selectable->GetCommand());

  // Leave only a single item "Three" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "Three".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(3, first_selectable->GetCommand());
  // The last selectable item should be item "Three".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(3, last_selectable->GetCommand());

  // Leave only a single item ("Two") selected. It should be the first and the
  // last selectable item.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Verifies that the context menu bubble should prioritize its cached menu
// position (above or below the anchor) after its size updates
// (https://crbug.com/1126244).
TEST_F(MenuControllerTest, VerifyMenuBubblePositionAfterSizeChanges) {
  constexpr gfx::Rect monitor_bounds(0, 0, 500, 500);
  constexpr gfx::Size menu_size(100, 200);
  const gfx::Insets border_and_shadow_insets =
      GetBorderAndShadowInsets(/*is_submenu=*/false);

  // Calculate the suitable anchor point to ensure that if the menu shows below
  // the anchor point, the bottom of the menu should be one pixel off the
  // bottom of the display. It means that there is insufficient space for the
  // menu below the anchor.
  const gfx::Point anchor_point(monitor_bounds.width() / 2,
                                monitor_bounds.bottom() + 1 -
                                    menu_size.height() +
                                    border_and_shadow_insets.top());

  MenuBoundsOptions options;
  options.menu_anchor = MenuAnchorPosition::kBubbleRight;
  options.monitor_bounds = monitor_bounds;
  options.anchor_bounds = gfx::Rect(anchor_point, gfx::Size());

  // Case 1: There is insufficient space for the menu below `anchor_point` and
  // there is no cached menu position. The menu should show above the anchor.
  {
    options.menu_size = menu_size;
    ASSERT_GT(options.anchor_bounds.y() - border_and_shadow_insets.top() +
                  menu_size.height(),
              monitor_bounds.bottom());
    CalculateBubbleMenuBounds(options);
    EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
              menu_item()->ActualMenuPosition());
  }

  // Case 2: There is insufficient space for the menu below `anchor_point`. The
  // cached position is below the anchor. The menu should show above the anchor.
  {
    options.menu_size = menu_size;
    options.menu_position = MenuItemView::MenuPosition::kBelowBounds;
    CalculateBubbleMenuBounds(options);
    EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
              menu_item()->ActualMenuPosition());
  }

  // Case 3: There is enough space for the menu below `anchor_point`. The cached
  // menu position is above the anchor. The menu should show above the anchor.
  {
    // Shrink the menu size. Verify that there is enough space below the anchor
    // point now.
    constexpr gfx::Size updated_size(menu_size.width(), menu_size.height() / 2);
    options.menu_size = updated_size;
    EXPECT_LE(options.anchor_bounds.y() - border_and_shadow_insets.top() +
                  updated_size.height(),
              monitor_bounds.bottom());

    options.menu_position = MenuItemView::MenuPosition::kAboveBounds;
    CalculateBubbleMenuBounds(options);
    EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
              menu_item()->ActualMenuPosition());
  }
}

// Verifies that the context menu bubble position,
// MenuAnchorPosition::kBubbleBottomRight, does not shift as items are removed.
// The menu position will shift if items are added and the menu no longer fits
// in its previous position.
TEST_F(MenuControllerTest, VerifyContextMenuBubblePositionAfterSizeChanges) {
  constexpr gfx::Rect kMonitorBounds(0, 0, 500, 500);
  constexpr gfx::Size kMenuSize(100, 200);
  const gfx::Insets border_and_shadow_insets =
      GetBorderAndShadowInsets(/*is_submenu=*/false);

  // Calculate the suitable anchor point to ensure that if the menu shows below
  // the anchor point, the bottom of the menu should be one pixel off the
  // bottom of the display. It means that there is insufficient space for the
  // menu below the anchor.
  const gfx::Point anchor_point(kMonitorBounds.width() / 2,
                                kMonitorBounds.bottom() + 1 -
                                    kMenuSize.height() +
                                    border_and_shadow_insets.top());

  MenuBoundsOptions options;
  options.menu_anchor = MenuAnchorPosition::kBubbleBottomRight;
  options.monitor_bounds = kMonitorBounds;
  options.anchor_bounds = gfx::Rect(anchor_point, gfx::Size());

  // Case 1: There is insufficient space for the menu below `anchor_point` and
  // there is no cached menu position. The menu should show above the anchor.
  {
    options.menu_size = kMenuSize;
    ASSERT_GT(options.anchor_bounds.y() - border_and_shadow_insets.top() +
                  kMenuSize.height(),
              kMonitorBounds.bottom());
    CalculateBubbleMenuBounds(options);
    EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
              menu_item()->ActualMenuPosition());
  }

  // Case 2: There is insufficient space for the menu below `anchor_point`. The
  // cached position is below the anchor. The menu should show above the anchor
  // point.
  {
    options.menu_position = MenuItemView::MenuPosition::kBelowBounds;
    CalculateBubbleMenuBounds(options);
    EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
              menu_item()->ActualMenuPosition());
  }

  // Case 3: There is enough space for the menu below `anchor_point`. The cached
  // menu position is above the anchor. The menu should show above the anchor.
  {
    // Shrink the menu size. Verify that there is enough space below the anchor
    // point now.
    constexpr gfx::Size kUpdatedSize(kMenuSize.width(), kMenuSize.height() / 2);
    options.menu_size = kUpdatedSize;
    EXPECT_LE(options.anchor_bounds.y() - border_and_shadow_insets.top() +
                  kUpdatedSize.height(),
              kMonitorBounds.bottom());

    options.menu_position = MenuItemView::MenuPosition::kAboveBounds;
    CalculateBubbleMenuBounds(options);
    EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
              menu_item()->ActualMenuPosition());
  }

  // Case 4: There is enough space for the menu below `anchor_point`. The cached
  // menu position is below the anchor. The menu should show below the anchor.
  {
    options.menu_position = MenuItemView::MenuPosition::kBelowBounds;
    CalculateBubbleMenuBounds(options);
    EXPECT_EQ(MenuItemView::MenuPosition::kBelowBounds,
              menu_item()->ActualMenuPosition());
  }
}

// Tests that opening the menu and pressing 'Home' selects the first menu item.
TEST_F(MenuControllerTest, FirstSelectedItem) {
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the first menu item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Fake initial root item selection and submenu showing.
  SetPendingStateItem(menu_item());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Select the first menu item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the last item.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(3));
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the first menu item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening the menu and pressing 'End' selects the last enabled menu
// item.
TEST_F(MenuControllerTest, LastSelectedItem) {
  // Fake initial root item selection and submenu showing.
  SetPendingStateItem(menu_item());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the last item.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(3));
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the first item.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// MenuController tests which set expectations about how menu item selection
// behaves should verify test cases work as intended for all supported selection
// mechanisms.
class MenuControllerSelectionTest : public MenuControllerTest {
 public:
  MenuControllerSelectionTest() = default;
  ~MenuControllerSelectionTest() override = default;

 protected:
  // Models a mechanism by which menu item selection can be incremented and/or
  // decremented.
  struct SelectionMechanism {
    base::RepeatingClosure IncrementSelection;
    base::RepeatingClosure DecrementSelection;
  };

  // Returns all mechanisms by which menu item selection can be incremented
  // and/or decremented.
  const std::vector<SelectionMechanism>& selection_mechanisms() {
    return selection_mechanisms_;
  }

 private:
  const std::vector<SelectionMechanism> selection_mechanisms_ = {
      // Updates selection via IncrementSelection()/DecrementSelection().
      SelectionMechanism{
          base::BindLambdaForTesting([this]() { IncrementSelection(); }),
          base::BindLambdaForTesting([this]() { DecrementSelection(); })},
      // Updates selection via down/up arrow keys.
      SelectionMechanism{
          base::BindLambdaForTesting([this]() { DispatchKey(ui::VKEY_DOWN); }),
          base::BindLambdaForTesting([this]() { DispatchKey(ui::VKEY_UP); })},
      // Updates selection via next/prior keys.
      SelectionMechanism{
          base::BindLambdaForTesting([this]() { DispatchKey(ui::VKEY_NEXT); }),
          base::BindLambdaForTesting(
              [this]() { DispatchKey(ui::VKEY_PRIOR); })}};
};

// Tests that opening menu and exercising various mechanisms to update selection
// iterates over enabled items.
TEST_F(MenuControllerSelectionTest, NextSelectedItem) {
  for (const auto& selection_mechanism : selection_mechanisms()) {
    // Disabling the item "Three" gets it skipped when using keyboard to
    // navigate.
    menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);

    // Fake initial hot selection.
    SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
    EXPECT_EQ(1, pending_state_item()->GetCommand());

    // Move down in the menu.
    // Select next item.
    selection_mechanism.IncrementSelection.Run();
    EXPECT_EQ(2, pending_state_item()->GetCommand());

    // Skip disabled item.
    selection_mechanism.IncrementSelection.Run();
    EXPECT_EQ(4, pending_state_item()->GetCommand());

    if (SelectionWraps()) {
      // Wrap around.
      selection_mechanism.IncrementSelection.Run();
      EXPECT_EQ(1, pending_state_item()->GetCommand());

      // Move up in the menu.
      // Wrap around.
      selection_mechanism.DecrementSelection.Run();
      EXPECT_EQ(4, pending_state_item()->GetCommand());
    } else {
      // Don't wrap.
      selection_mechanism.IncrementSelection.Run();
      EXPECT_EQ(4, pending_state_item()->GetCommand());
    }

    // Skip disabled item.
    selection_mechanism.DecrementSelection.Run();
    EXPECT_EQ(2, pending_state_item()->GetCommand());

    // Select previous item.
    selection_mechanism.DecrementSelection.Run();
    EXPECT_EQ(1, pending_state_item()->GetCommand());

    // Clear references in menu controller to the menu item that is going
    // away.
    ResetSelection();
  }
}

// Tests that opening menu and exercising various mechanisms to decrement
// selection selects the last enabled menu item.
TEST_F(MenuControllerSelectionTest, PreviousSelectedItem) {
  for (const auto& selection_mechanism : selection_mechanisms()) {
    // Disabling the item "Four" gets it skipped when using keyboard to
    // navigate.
    menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);

    // Fake initial root item selection and submenu showing.
    SetPendingStateItem(menu_item());
    EXPECT_EQ(0, pending_state_item()->GetCommand());

    // Move up and select a previous (in our case the last enabled) item.
    selection_mechanism.DecrementSelection.Run();
    EXPECT_EQ(3, pending_state_item()->GetCommand());

    // Clear references in menu controller to the menu item that is going
    // away.
    ResetSelection();
  }
}

// Tests that the APIs related to the current selected item work correctly.
TEST_F(MenuControllerTest, CurrentSelectedItem) {
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the first menu-item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(pending_state_item(), menu_controller()->GetSelectedMenuItem());

  // The API should let the submenu stay open if already so, but clear any
  // selections within it.
  EXPECT_TRUE(IsShowing());
  EXPECT_EQ(1, pending_state_item()->GetCommand());
  menu_controller()->SelectItemAndOpenSubmenu(menu_item());
  EXPECT_TRUE(IsShowing());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and calling SelectByChar works correctly.
TEST_F(MenuControllerTest, SelectByChar) {
  SetComboboxType(MenuController::ComboboxType::kReadonly);

  // Handle null character should do nothing.
  SelectByChar(0);
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

TEST_F(MenuControllerTest, SelectChildButtonView) {
  AddButtonMenuItems(/*single_child=*/false);
  View* buttons_view = menu_item()->GetSubmenu()->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  Button* button1 = Button::AsButton(buttons_view->children()[0]);
  ASSERT_NE(nullptr, button1);
  Button* button2 = Button::AsButton(buttons_view->children()[1]);
  ASSERT_NE(nullptr, button2);
  Button* button3 = Button::AsButton(buttons_view->children()[2]);
  ASSERT_NE(nullptr, button2);

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button1|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button2|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button3|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_TRUE(button3->IsHotTracked());

  // Move a mouse to hot track the |button1|.
  SubmenuView* sub_menu = menu_item()->GetSubmenu();
  gfx::Point location(button1->GetBoundsInScreen().CenterPoint());
  View::ConvertPointFromScreen(sub_menu->GetScrollViewContainer(), &location);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location,
                       ui::EventTimeForNow(), 0, 0);
  ProcessMouseMoved(sub_menu, event);
  EXPECT_EQ(button1, GetHotButton());
  EXPECT_TRUE(button1->IsHotTracked());

  // Incrementing selection should move hot tracking to the second button (next
  // after the first button).
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment selection twice to wrap around.
  IncrementSelection();
  IncrementSelection();
  if (SelectionWraps())
    EXPECT_EQ(1, pending_state_item()->GetCommand());
  else
    EXPECT_EQ(5, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

TEST_F(MenuControllerTest, DeleteChildButtonView) {
  AddButtonMenuItems(/*single_child=*/false);

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  View* buttons_view = menu_item()->GetSubmenu()->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  Button* button1 = Button::AsButton(buttons_view->children()[0]);
  ASSERT_NE(nullptr, button1);
  Button* button2 = Button::AsButton(buttons_view->children()[1]);
  ASSERT_NE(nullptr, button2);
  Button* button3 = Button::AsButton(buttons_view->children()[2]);
  ASSERT_NE(nullptr, button2);
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment twice to move selection to |button2|.
  IncrementSelection();
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Delete |button2| while it is hot-tracked.
  // This should update MenuController via ViewHierarchyChanged and reset
  // |hot_button_|.
  delete button2;

  // Incrementing selection should now set hot-tracked item to |button1|.
  // It should not crash.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());
}

// Verifies that the child button is hot tracked after the host menu item is
// selected by `MenuController::SelectItemAndOpenSubmenu()`.
TEST_F(MenuControllerTest, ChildButtonHotTrackedAfterMenuItemSelection) {
  // Add a menu item which owns a button as child.
  MenuItemView* hosting_menu_item = AddButtonMenuItems(/*single_child=*/true);
  ASSERT_FALSE(hosting_menu_item->IsSelected());
  const Button* button = Button::AsButton(hosting_menu_item->children()[0]);
  EXPECT_FALSE(button->IsHotTracked());

  menu_controller()->SelectItemAndOpenSubmenu(hosting_menu_item);
  EXPECT_TRUE(hosting_menu_item->IsSelected());
  EXPECT_TRUE(button->IsHotTracked());
}

// Verifies that the child button of the menu item which is under mouse hovering
// is hot tracked (https://crbug.com/1135000).
TEST_F(MenuControllerTest, ChildButtonHotTrackedAfterMouseMove) {
  // Add a menu item which owns a button as child.
  MenuItemView* hosting_menu_item = AddButtonMenuItems(/*single_child=*/true);
  const Button* button = Button::AsButton(hosting_menu_item->children()[0]);
  EXPECT_FALSE(button->IsHotTracked());

  SubmenuView* sub_menu = menu_item()->GetSubmenu();
  gfx::Point location(button->GetBoundsInScreen().CenterPoint());
  View::ConvertPointFromScreen(sub_menu->GetScrollViewContainer(), &location);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location,
                       ui::EventTimeForNow(), 0, 0);
  ProcessMouseMoved(sub_menu, event);

  // After the mouse moves to `button`, `button` should be hot tracked.
  EXPECT_EQ(button, GetHotButton());
  EXPECT_TRUE(button->IsHotTracked());
}

// Creates a menu with Button child views, simulates running a nested
// menu and tests that existing the nested run restores hot-tracked child view.
TEST_F(MenuControllerTest, ChildButtonHotTrackedWhenNested) {
  AddButtonMenuItems(/*single_child=*/false);

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  View* buttons_view = menu_item()->GetSubmenu()->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  Button* button1 = Button::AsButton(buttons_view->children()[0]);
  ASSERT_NE(nullptr, button1);
  Button* button2 = Button::AsButton(buttons_view->children()[1]);
  ASSERT_NE(nullptr, button2);
  Button* button3 = Button::AsButton(buttons_view->children()[2]);
  ASSERT_NE(nullptr, button2);
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment twice to move selection to |button2|.
  IncrementSelection();
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());
  EXPECT_EQ(button2, GetHotButton());

  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  // |button2| should stay in hot-tracked state but menu controller should not
  // track it anymore (preventing resetting hot-tracked state when changing
  // selection while a nested run is active).
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_EQ(nullptr, GetHotButton());

  // Setting hot-tracked button while nested should get reverted when nested
  // menu run ends.
  SetHotTrackedButton(button1);
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_EQ(button1, GetHotButton());

  // Setting the hot tracked state twice on the same button via the
  // menu controller should still set the hot tracked state on the button again.
  button1->SetHotTracked(false);
  SetHotTrackedButton(button1);
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_EQ(button1, GetHotButton());

  ExitMenuRun();
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_EQ(button2, GetHotButton());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when Accept is called.
TEST_F(MenuControllerTest, AsynchronousAccept) {
  views::test::DisableMenuClosureAnimations();

  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  MenuItemView* accepted = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  const int kEventFlags = 42;
  Accept(accepted, kEventFlags);

  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(accepted, delegate->on_menu_closed_menu());
  EXPECT_EQ(kEventFlags, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when CancelAll is called.
TEST_F(MenuControllerTest, AsynchronousCancelAll) {
  MenuController* controller = menu_controller();

  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  controller->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(0, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that canceling a nested menu restores the previous
// MenuControllerDelegate, and notifies each delegate.
TEST_F(MenuControllerTest, AsynchronousNestedDelegate) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  controller->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  controller->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(delegate, GetCurrentDelegate());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that dropping within an asynchronous menu stops the menu from showing
// and does not notify the controller.
TEST_F(MenuControllerTest, AsynchronousPerformDrop) {
  MenuController* controller = menu_controller();
  SubmenuView* source = menu_item()->GetSubmenu();
  MenuItemView* target = source->GetMenuItemAt(0);

  SetDropMenuItem(target, MenuDelegate::DropPosition::kAfter);

  ui::OSExchangeData drop_data;
  gfx::PointF location(target->origin());
  ui::DropTargetEvent target_event(drop_data, location, location,
                                   ui::DragDropTypes::DRAG_MOVE);
  auto drop_cb = controller->GetDropCallback(source, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op);

  TestMenuDelegate* menu_delegate =
      static_cast<TestMenuDelegate*>(target->GetDelegate());
  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_TRUE(menu_delegate->is_drop_performed());
  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(0, controller_delegate->on_menu_closed_called());
}

// Tests that dragging within an asynchronous menu notifies the
// MenuControllerDelegate for shutdown.
TEST_F(MenuControllerTest, AsynchronousDragComplete) {
  MenuController* controller = menu_controller();
  TestDragCompleteThenDestroyOnMenuClosed();

  controller->OnDragWillStart();
  controller->OnDragComplete(true);

  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_EQ(1, controller_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, controller_delegate->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            controller_delegate->on_menu_closed_notify_type());
}

// Tests that if Cancel is called during a drag, that OnMenuClosed is still
// notified when the drag completes.
TEST_F(MenuControllerTest, AsynchronousCancelDuringDrag) {
  MenuController* controller = menu_controller();
  TestDragCompleteThenDestroyOnMenuClosed();

  controller->OnDragWillStart();
  controller->Cancel(MenuController::ExitType::kAll);
  controller->OnDragComplete(true);

  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_EQ(1, controller_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, controller_delegate->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            controller_delegate->on_menu_closed_notify_type());
}

// Tests that if a menu is destroyed while drag operations are occurring, that
// the MenuHost does not crash as the drag completes.
TEST_F(MenuControllerTest, AsynchronousDragHostDeleted) {
  SubmenuView* submenu = menu_item()->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = menu_item()->bounds();
  params.do_capture = false;
  submenu->ShowAt(params);
  MenuHost* host = GetMenuHost(submenu);
  MenuHostOnDragWillStart(host);
  submenu->Close();
  DestroyMenuItem();
  MenuHostOnDragComplete(host);
}

// Tests that getting the drop callback stops the menu from showing and
// does not notify the controller.
TEST_F(MenuControllerTest, AsycDropCallback) {
  MenuController* controller = menu_controller();
  SubmenuView* source = menu_item()->GetSubmenu();
  MenuItemView* target = source->GetMenuItemAt(0);

  SetDropMenuItem(target, MenuDelegate::DropPosition::kAfter);

  ui::OSExchangeData drop_data;
  gfx::PointF location(target->origin());
  ui::DropTargetEvent target_event(drop_data, location, location,
                                   ui::DragDropTypes::DRAG_MOVE);
  auto drop_cb = controller->GetDropCallback(source, target_event);

  TestMenuDelegate* menu_delegate =
      static_cast<TestMenuDelegate*>(target->GetDelegate());
  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_FALSE(menu_delegate->is_drop_performed());
  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(0, controller_delegate->on_menu_closed_called());

  ui::mojom::DragOperation output_drag_op;
  std::move(drop_cb).Run(target_event, output_drag_op);
  EXPECT_TRUE(menu_delegate->is_drop_performed());
}

// Widget destruction and cleanup occurs on the MessageLoop after the
// MenuController has been destroyed. A MenuHostRootView should not attempt to
// access a destroyed MenuController. This test should not cause a crash.
TEST_F(MenuControllerTest, HostReceivesInputBeforeDestruction) {
  SubmenuView* submenu = menu_item()->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = menu_item()->bounds();
  params.do_capture = false;
  submenu->ShowAt(params);
  gfx::Point location(submenu->bounds().bottom_right());
  location.Offset(1, 1);

  MenuHost* host = GetMenuHost(submenu);
  // Normally created as the full Widget is brought up. Explicitly created here
  // for testing.
  std::unique_ptr<MenuHostRootView> root_view(CreateMenuHostRootView(host));
  DestroyMenuController();

  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  // This should not attempt to access the destroyed MenuController and should
  // not crash.
  root_view->OnMouseMoved(event);
}

// Tests that an asynchronous menu nested within an asynchronous menu closes
// both menus, and notifies both delegates.
TEST_F(MenuControllerTest, DoubleAsynchronousNested) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  // Nested run
  controller->AddNestedDelegate(nested_delegate.get());
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  controller->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that setting send_gesture_events_to_owner flag forwards gesture events
// to owner and the forwarding stops when the current gesture sequence ends.
TEST_F(MenuControllerTest, PreserveGestureForOwner) {
  MenuController* controller = menu_controller();
  MenuItemView* item = menu_item();
  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kBottomCenter, false, false);
  SubmenuView* sub_menu = item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = gfx::Rect(0, 0, 100, 100);
  params.do_capture = true;
  sub_menu->ShowAt(params);

  gfx::Point location(sub_menu->bounds().bottom_left().x(),
                      sub_menu->bounds().bottom_left().y() + 10);
  ui::GestureEvent event(location.x(), location.y(), 0, ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  // Gesture events should not be forwarded if the flag is not set.
  EXPECT_EQ(CountOwnerOnGestureEvent(), 0);
  EXPECT_FALSE(controller->send_gesture_events_to_owner());
  controller->OnGestureEvent(sub_menu, &event);
  EXPECT_EQ(CountOwnerOnGestureEvent(), 0);

  // The menu's owner should receive gestures triggered outside the menu.
  controller->set_send_gesture_events_to_owner(true);
  controller->OnGestureEvent(sub_menu, &event);
  EXPECT_EQ(CountOwnerOnGestureEvent(), 1);

  ui::GestureEvent event2(location.x(), location.y(), 0, ui::EventTimeForNow(),
                          ui::GestureEventDetails(ui::ET_GESTURE_END));

  controller->OnGestureEvent(sub_menu, &event2);
  EXPECT_EQ(CountOwnerOnGestureEvent(), 2);

  // ET_GESTURE_END resets the |send_gesture_events_to_owner_| flag, so further
  // gesture events should not be sent to the owner.
  controller->OnGestureEvent(sub_menu, &event2);
  EXPECT_EQ(CountOwnerOnGestureEvent(), 2);
}

#if defined(USE_AURA)
// Tests that setting `send_gesture_events_to_owner` flag forwards gesture
// events to the NativeView specified for gestures and not the owner's
// NativeView.
TEST_F(MenuControllerTest, ForwardsEventsToNativeViewForGestures) {
  aura::test::EventCountDelegate child_delegate;
  auto child_window = std::make_unique<aura::Window>(&child_delegate);
  child_window->Init(ui::LAYER_TEXTURED);
  owner()->GetNativeView()->AddChild(child_window.get());

  MenuController* controller = menu_controller();
  MenuItemView* item = menu_item();

  // Ensure menu is closed before running with the menu with `child_window` as
  // the NativeView for gestures.
  controller->Cancel(MenuController::ExitType::kAll);

  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kBottomCenter, false, false,
                  child_window.get());
  SubmenuView* sub_menu = item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = item->bounds();
  params.do_capture = false;
  params.native_view_for_gestures = child_window.get();
  sub_menu->ShowAt(params);

  gfx::Point location(sub_menu->bounds().bottom_left().x(),
                      sub_menu->bounds().bottom_left().y() + 10);
  ui::GestureEvent event(location.x(), location.y(), 0, ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  // Gesture events should not be forwarded to either the `child_window` or the
  // hosts native window if the flag is not set.
  EXPECT_EQ(0, CountOwnerOnGestureEvent());
  EXPECT_EQ(0, child_delegate.GetGestureCountAndReset());
  EXPECT_FALSE(controller->send_gesture_events_to_owner());
  controller->OnGestureEvent(sub_menu, &event);
  EXPECT_EQ(0, CountOwnerOnGestureEvent());
  EXPECT_EQ(0, child_delegate.GetGestureCountAndReset());

  // The `child_window` should receive gestures triggered outside the menu.
  controller->set_send_gesture_events_to_owner(true);
  controller->OnGestureEvent(sub_menu, &event);
  EXPECT_EQ(0, CountOwnerOnGestureEvent());
  EXPECT_EQ(1, child_delegate.GetGestureCountAndReset());

  ui::GestureEvent event2(location.x(), location.y(), 0, ui::EventTimeForNow(),
                          ui::GestureEventDetails(ui::ET_GESTURE_END));

  controller->OnGestureEvent(sub_menu, &event2);
  EXPECT_EQ(0, CountOwnerOnGestureEvent());
  EXPECT_EQ(1, child_delegate.GetGestureCountAndReset());

  // ET_GESTURE_END resets the `send_gesture_events_to_owner_` flag, so further
  // gesture events should not be sent to the `child_window`.
  controller->OnGestureEvent(sub_menu, &event2);
  EXPECT_EQ(0, CountOwnerOnGestureEvent());
  EXPECT_EQ(0, child_delegate.GetGestureCountAndReset());
}
#endif

// Tests that touch outside menu does not closes the menu when forwarding
// gesture events to owner.
TEST_F(MenuControllerTest, NoTouchCloseWhenSendingGesturesToOwner) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();

  // Owner wants the gesture events.
  controller->set_send_gesture_events_to_owner(true);

  // Show a sub menu and touch outside of it.
  MenuItemView* item = menu_item();
  SubmenuView* sub_menu = item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = item->bounds();
  params.do_capture = false;
  sub_menu->ShowAt(params);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  controller->OnTouchEvent(sub_menu, &touch_event);

  // Menu should still be visible.
  EXPECT_TRUE(IsShowing());

  // The current gesture sequence ends.
  ui::GestureEvent gesture_end_event(
      location.x(), location.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_END));
  controller->OnGestureEvent(sub_menu, &gesture_end_event);

  // Touch outside again and menu should be closed.
  controller->OnTouchEvent(sub_menu, &touch_event);
  views::test::WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that a nested menu does not crash when trying to repost events that
// occur outside of the bounds of the menu. Instead a proper shutdown should
// occur.
TEST_F(MenuControllerTest, AsynchronousRepostEvent) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  controller->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a pointer selection. However have the event
  // occur outside of the bounds of the entire menu.
  SubmenuView* sub_menu = item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = item->bounds();
  params.do_capture = false;
  sub_menu->ShowAt(params);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  // When attempting to select outside of all menus this should lead to a
  // shutdown. This should not crash while attempting to repost the event.
  SetSelectionOnPointerDown(sub_menu, &event);
  views::test::WaitForMenuClosureAnimation();

  EXPECT_EQ(delegate, GetCurrentDelegate());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that an asynchronous menu reposts touch events that occur outside of
// the bounds of the menu, and that the menu closes.
TEST_F(MenuControllerTest, AsynchronousTouchEventRepostEvent) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();

  // Show a sub menu to target with a touch event. However have the event occur
  // outside of the bounds of the entire menu.
  MenuItemView* item = menu_item();
  SubmenuView* sub_menu = item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = item->bounds();
  params.do_capture = false;
  sub_menu->ShowAt(params);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::TouchEvent event(ui::ET_TOUCH_PRESSED, location, ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  controller->OnTouchEvent(sub_menu, &event);
  views::test::WaitForMenuClosureAnimation();

  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(0, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that having the MenuController deleted during RepostEvent does not
// cause a crash. ASAN bots should not detect use-after-free in MenuController.
TEST_F(MenuControllerTest, AsynchronousRepostEventDeletesController) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  controller->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a pointer selection. However have the event
  // occur outside of the bounds of the entire menu.
  SubmenuView* sub_menu = item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = item->bounds();
  params.do_capture = true;
  sub_menu->ShowAt(params);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  // This will lead to MenuController being deleted during the event repost.
  // The remainder of this test, and TearDown should not crash.
  DestroyMenuControllerOnMenuClosed(nested_delegate.get());
  // When attempting to select outside of all menus this should lead to a
  // shutdown. This should not crash while attempting to repost the event.
  SetSelectionOnPointerDown(sub_menu, &event);
  views::test::WaitForMenuClosureAnimation();

  // Close to remove observers before test TearDown
  sub_menu->Close();
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that having the MenuController deleted during OnGestureEvent does not
// cause a crash. ASAN bots should not detect use-after-free in MenuController.
TEST_F(MenuControllerTest, AsynchronousGestureDeletesController) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  controller->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a tap event.
  SubmenuView* sub_menu = item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = gfx::Rect(0, 0, 100, 100);
  params.do_capture = true;
  sub_menu->ShowAt(params);

  gfx::Point location(sub_menu->bounds().CenterPoint());
  ui::GestureEvent event(location.x(), location.y(), 0, ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  // This will lead to MenuController being deleted during the processing of the
  // gesture event. The remainder of this test, and TearDown should not crash.
  DestroyMenuControllerOnMenuClosed(nested_delegate.get());
  controller->OnGestureEvent(sub_menu, &event);
  views::test::WaitForMenuClosureAnimation();

  // Close to remove observers before test TearDown
  sub_menu->Close();
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Test that the menu is properly placed where it best fits.
TEST_F(MenuControllerTest, CalculateMenuBoundsBestFitTest) {
  MenuBoundsOptions options;
  gfx::Rect expected;
  const bool ignore_screen_bounds_for_menus =
      ShouldIgnoreScreenBoundsForMenus();

  // Fits in all locations -> placed below.
  options.anchor_bounds =
      gfx::Rect(options.menu_size.width(), options.menu_size.height(), 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.anchor_bounds.bottom() + options.menu_size.height());
  expected =
      gfx::Rect(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                options.menu_size.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits above and to both sides -> placed above.
  options.anchor_bounds =
      gfx::Rect(options.menu_size.width(), options.menu_size.height(), 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.anchor_bounds.bottom());
  if (ignore_screen_bounds_for_menus) {
    expected = gfx::Rect(options.anchor_bounds.origin(), options.menu_size);
  } else {
    expected = gfx::Rect(options.anchor_bounds.x(),
                         options.anchor_bounds.y() - options.menu_size.height(),
                         options.menu_size.width(), options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits on both sides, prefer right -> placed right.
  options.anchor_bounds = gfx::Rect(options.menu_size.width(),
                                    options.menu_size.height() / 2, 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected = gfx::Rect(options.anchor_bounds.origin(), options.menu_size);
  } else {
    expected =
        gfx::Rect(options.anchor_bounds.right(),
                  options.monitor_bounds.bottom() - options.menu_size.height(),
                  options.menu_size.width(), options.menu_size.height());
  }

  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits only on left -> placed left.
  options.anchor_bounds = gfx::Rect(options.menu_size.width(),
                                    options.menu_size.height() / 2, 0, 0);
  options.monitor_bounds = gfx::Rect(0, 0, options.anchor_bounds.right(),
                                     options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected = gfx::Rect(options.anchor_bounds.origin(), options.menu_size);
  } else {
    expected =
        gfx::Rect(options.anchor_bounds.x() - options.menu_size.width(),
                  options.monitor_bounds.bottom() - options.menu_size.height(),
                  options.menu_size.width(), options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits on both sides, prefer left -> placed left.
  options.menu_anchor = MenuAnchorPosition::kTopRight;
  options.anchor_bounds = gfx::Rect(options.menu_size.width(),
                                    options.menu_size.height() / 2, 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected =
        gfx::Rect({options.anchor_bounds.right() - options.menu_size.width(),
                   options.anchor_bounds.origin().y()},
                  options.menu_size);
  } else {
    expected =
        gfx::Rect(options.anchor_bounds.x() - options.menu_size.width(),
                  options.monitor_bounds.bottom() - options.menu_size.height(),
                  options.menu_size.width(), options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits only on right -> placed right.
  options.anchor_bounds = gfx::Rect(0, options.menu_size.height() / 2, 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected =
        gfx::Rect({options.anchor_bounds.right() - options.menu_size.width(),
                   options.anchor_bounds.origin().y()},
                  options.menu_size);
  } else {
    expected =
        gfx::Rect(options.anchor_bounds.right(),
                  options.monitor_bounds.bottom() - options.menu_size.height(),
                  options.menu_size.width(), options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

// Tests that the menu is properly placed according to its anchor.
TEST_F(MenuControllerTest, CalculateMenuBoundsAnchorTest) {
  MenuBoundsOptions options;
  gfx::Rect expected;

  options.menu_anchor = MenuAnchorPosition::kTopLeft;
  expected =
      gfx::Rect(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                options.menu_size.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_anchor = MenuAnchorPosition::kTopRight;
  expected =
      gfx::Rect(options.anchor_bounds.right() - options.menu_size.width(),
                options.anchor_bounds.bottom(), options.menu_size.width(),
                options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Menu will be placed above or below with an offset.
  options.menu_anchor = MenuAnchorPosition::kBottomCenter;
  const int kTouchYPadding = 15;

  // Menu fits above -> placed above.
  expected = gfx::Rect(
      options.anchor_bounds.x() +
          (options.anchor_bounds.width() - options.menu_size.width()) / 2,
      options.anchor_bounds.y() - options.menu_size.height() - kTouchYPadding,
      options.menu_size.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Menu does not fit above -> placed below.
  options.anchor_bounds = gfx::Rect(options.menu_size.height() / 2,
                                    options.menu_size.width(), 0, 0);
  if (ShouldIgnoreScreenBoundsForMenus()) {
    expected = gfx::Rect(
        options.anchor_bounds.x() +
            (options.anchor_bounds.width() - options.menu_size.width()) / 2,
        options.anchor_bounds.y() - options.anchor_bounds.bottom() -
            kTouchYPadding,
        options.menu_size.width(), options.menu_size.height());
  } else {
    expected = gfx::Rect(
        options.anchor_bounds.x() +
            (options.anchor_bounds.width() - options.menu_size.width()) / 2,
        options.anchor_bounds.y() + kTouchYPadding, options.menu_size.width(),
        options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

// Regression test for https://crbug.com/1217711
TEST_F(MenuControllerTest, MenuAnchorPositionFlippedInRtl) {
  ASSERT_FALSE(base::i18n::IsRTL());

  // Test the AdjustAnchorPositionForRtl() method directly, rather than running
  // the menu, because it's awkward to access the menu's window. Also, the menu
  // bounds are already tested separately.
  EXPECT_EQ(MenuAnchorPosition::kTopLeft,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kTopLeft));
  EXPECT_EQ(MenuAnchorPosition::kTopRight,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kTopRight));
  EXPECT_EQ(MenuAnchorPosition::kBubbleTopLeft,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleTopLeft));
  EXPECT_EQ(MenuAnchorPosition::kBubbleTopRight,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleTopRight));
  EXPECT_EQ(MenuAnchorPosition::kBubbleLeft,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleLeft));
  EXPECT_EQ(MenuAnchorPosition::kBubbleRight,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleRight));
  EXPECT_EQ(MenuAnchorPosition::kBubbleBottomLeft,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleBottomLeft));
  EXPECT_EQ(MenuAnchorPosition::kBubbleBottomRight,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleBottomRight));

  base::i18n::SetRTLForTesting(true);

  // Anchor positions are left/right flipped in RTL.
  EXPECT_EQ(MenuAnchorPosition::kTopLeft,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kTopRight));
  EXPECT_EQ(MenuAnchorPosition::kTopRight,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kTopLeft));
  EXPECT_EQ(MenuAnchorPosition::kBubbleTopLeft,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleTopRight));
  EXPECT_EQ(MenuAnchorPosition::kBubbleTopRight,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleTopLeft));
  EXPECT_EQ(MenuAnchorPosition::kBubbleLeft,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleRight));
  EXPECT_EQ(MenuAnchorPosition::kBubbleRight,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleLeft));
  EXPECT_EQ(MenuAnchorPosition::kBubbleBottomLeft,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleBottomRight));
  EXPECT_EQ(MenuAnchorPosition::kBubbleBottomRight,
            AdjustAnchorPositionForRtl(MenuAnchorPosition::kBubbleBottomLeft));

  base::i18n::SetRTLForTesting(false);
}

TEST_F(MenuControllerTest, CalculateMenuBoundsMonitorFitTest) {
  MenuBoundsOptions options;
  gfx::Rect expected;
  options.monitor_bounds = gfx::Rect(0, 0, 100, 100);
  options.anchor_bounds = gfx::Rect();

  options.menu_size = gfx::Size(options.monitor_bounds.width() / 2,
                                options.monitor_bounds.height() * 2);
  expected =
      gfx::Rect(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                options.menu_size.width(), options.monitor_bounds.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_size = gfx::Size(options.monitor_bounds.width() * 2,
                                options.monitor_bounds.height() / 2);
  expected =
      gfx::Rect(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                options.monitor_bounds.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_size = gfx::Size(options.monitor_bounds.width() * 2,
                                options.monitor_bounds.height() * 2);
  expected = gfx::Rect(
      options.anchor_bounds.x(), options.anchor_bounds.bottom(),
      options.monitor_bounds.width(), options.monitor_bounds.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

// Test that menus show up on screen with non-zero sized anchors.
TEST_P(MenuControllerTest, TestMenuFitsOnScreen) {
  const int display_size = 500;
  // Simulate multiple display layouts.
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      const gfx::Rect monitor_bounds(x * display_size, y * display_size,
                                     display_size, display_size);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleTopLeft, monitor_bounds);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleTopRight, monitor_bounds);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleLeft, monitor_bounds);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleRight, monitor_bounds);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleBottomLeft,
                           monitor_bounds);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleBottomRight,
                           monitor_bounds);
    }
  }
}

// Test that menus show up on screen with zero sized anchors.
TEST_P(MenuControllerTest, TestMenuFitsOnScreenSmallAnchor) {
  const int display_size = 500;
  // Simulate multiple display layouts.
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      const gfx::Rect monitor_bounds(x * display_size, y * display_size,
                                     display_size, display_size);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleTopLeft,
                                      monitor_bounds);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleTopRight,
                                      monitor_bounds);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleLeft,
                                      monitor_bounds);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleRight,
                                      monitor_bounds);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleBottomLeft,
                                      monitor_bounds);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleBottomRight,
                                      monitor_bounds);
    }
  }
}

// Test that menus fit a small screen.
TEST_P(MenuControllerTest, TestMenuFitsOnSmallScreen) {
  const int display_size = 500;

  // Simulate multiple display layouts.
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      const gfx::Rect monitor_bounds(x * display_size, y * display_size,
                                     display_size, display_size);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleTopLeft,
                                monitor_bounds);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleTopRight,
                                monitor_bounds);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleLeft,
                                monitor_bounds);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleRight,
                                monitor_bounds);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleBottomLeft,
                                monitor_bounds);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleBottomRight,
                                monitor_bounds);
    }
  }
}

// Test that submenus are displayed within the screen bounds on smaller screens.
TEST_P(MenuControllerTest, TestSubmenuFitsOnScreen) {
  menu_controller()->set_use_ash_system_ui_layout(true);
  MenuItemView* sub_item = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  sub_item->AppendMenuItem(11, u"Subitem.One");

  const int menu_width = MenuConfig::instance().touchable_menu_min_width;
  const gfx::Size parent_size(menu_width, menu_width);
  const gfx::Size parent_size_wide(menu_width * 2, menu_width);

  const int kDisplayWidth = parent_size.width() * 3;
  const int kDisplayHeight = parent_size.height() * 3;

  // For both kBubbleTopLeft and kBubbleTopRight.
  for (auto menu_position : {MenuAnchorPosition::kBubbleTopLeft,
                             MenuAnchorPosition::kBubbleTopRight}) {
    // Simulate multiple display layouts.
    for (int x = -1; x <= 1; x++) {
      for (int y = -1; y <= 1; y++) {
        const gfx::Rect monitor_bounds(x * kDisplayWidth, y * kDisplayHeight,
                                       kDisplayWidth, kDisplayHeight);

        const int x_min = monitor_bounds.x();
        const int x_max = monitor_bounds.right() - parent_size.width();
        const int x_mid = (x_min + x_max) / 2;
        const int x_qtr = x_min + (x_max - x_min) / 4;

        const int y_min = monitor_bounds.y();
        const int y_max = monitor_bounds.bottom() - parent_size.height();
        const int y_mid = (y_min + y_max) / 2;

        TestSubmenuFitsOnScreen(
            sub_item, monitor_bounds,
            gfx::Rect(gfx::Point(x_min, y_min), parent_size), menu_position);
        TestSubmenuFitsOnScreen(
            sub_item, monitor_bounds,
            gfx::Rect(gfx::Point(x_max, y_min), parent_size), menu_position);
        TestSubmenuFitsOnScreen(
            sub_item, monitor_bounds,
            gfx::Rect(gfx::Point(x_mid, y_min), parent_size), menu_position);
        TestSubmenuFitsOnScreen(
            sub_item, monitor_bounds,
            gfx::Rect(gfx::Point(x_min, y_mid), parent_size), menu_position);
        TestSubmenuFitsOnScreen(
            sub_item, monitor_bounds,
            gfx::Rect(gfx::Point(x_min, y_max), parent_size), menu_position);

        // Extra wide menu: test with insufficient room on both sides.
        TestSubmenuFitsOnScreen(
            sub_item, monitor_bounds,
            gfx::Rect(gfx::Point(x_qtr, y_min), parent_size_wide),
            menu_position);
      }
    }
  }
}

// Test that a menu that was originally drawn below the anchor does not get
// squished or move above the anchor when it grows vertically and horizontally
// beyond the monitor bounds.
TEST_F(MenuControllerTest, GrowingMenuMovesLaterallyNotVertically) {
  // We can't know the position of windows in Wayland. Thus, this case is not
  // valid for Wayland.
  if (ShouldIgnoreScreenBoundsForMenus())
    return;

  MenuBoundsOptions options;
  options.monitor_bounds = gfx::Rect(0, 0, 100, 100);
  // The anchor should be near the bottom right side of the screen.
  options.anchor_bounds = gfx::Rect(80, 70, 15, 10);
  // The menu should fit the available space, below the anchor.
  options.menu_size = gfx::Size(20, 20);

  // Ensure the menu is initially drawn below the bounds, and the MenuPosition
  // is set to MenuPosition::kBelowBounds;
  const gfx::Rect first_drawn_expected(80, 80, 20, 20);
  EXPECT_EQ(first_drawn_expected, CalculateMenuBounds(options));
  EXPECT_EQ(MenuItemView::MenuPosition::kBelowBounds,
            menu_item()->ActualMenuPosition());

  options.menu_position = MenuItemView::MenuPosition::kBelowBounds;

  // The menu bounds are larger than the remaining space on the monitor. This
  // simulates the case where the menu has been grown vertically and
  // horizontally to where it would no longer fit on the screen.
  options.menu_size = gfx::Size(50, 50);

  // The menu bounds should move left to show the wider menu, and grow to fill
  // the remaining vertical space without moving upwards.
  const gfx::Rect final_expected(50, 80, 50, 20);
  EXPECT_EQ(final_expected, CalculateMenuBounds(options));
}

#if defined(USE_AURA)
// This tests that mouse moved events from the initial position of the mouse
// when the menu was shown don't select the menu item at the mouse position.
TEST_F(MenuControllerTest, MouseAtMenuItemOnShow) {
  // Most tests create an already shown menu but this test needs one that's
  // not shown, so it can show it. The mouse position is remembered when
  // the menu is shown.
  std::unique_ptr<TestMenuItemViewNotShown> menu_item(
      new TestMenuItemViewNotShown(menu_delegate()));
  MenuItemView* first_item = menu_item->AppendMenuItem(1, u"One");
  menu_item->AppendMenuItem(2, u"Two");
  menu_item->SetController(menu_controller());

  // Move the mouse to where the first menu item will be shown,
  // and show the menu.
  gfx::Size item_size = first_item->CalculatePreferredSize();
  gfx::Point location(item_size.width() / 2, item_size.height() / 2);
  GetRootWindow(owner())->MoveCursorTo(location);
  menu_controller()->Run(owner(), nullptr, menu_item.get(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Synthesize an event at the mouse position when the menu was opened.
  // It should be ignored, and selected item shouldn't change.
  SubmenuView* sub_menu = menu_item->GetSubmenu();
  View::ConvertPointFromScreen(sub_menu->GetScrollViewContainer(), &location);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location,
                       ui::EventTimeForNow(), 0, 0);
  ProcessMouseMoved(sub_menu, event);
  EXPECT_EQ(0, pending_state_item()->GetCommand());
  // Synthesize an event at a slightly different mouse position. It
  // should cause the item under the cursor to be selected.
  location.Offset(0, 1);
  ui::MouseEvent second_event(ui::ET_MOUSE_MOVED, location, location,
                              ui::EventTimeForNow(), 0, 0);
  ProcessMouseMoved(sub_menu, second_event);
  EXPECT_EQ(1, pending_state_item()->GetCommand());
}

// Tests that when an asynchronous menu receives a cancel event, that it closes.
TEST_F(MenuControllerTest, AsynchronousCancelEvent) {
  ExitMenuRun();
  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(MenuController::ExitType::kNone, controller->exit_type());
  ui::CancelModeEvent cancel_event;
  event_generator()->Dispatch(&cancel_event);
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that menus without parent widgets do not crash in MenuPreTargetHandler.
TEST_F(MenuControllerTest, RunWithoutWidgetDoesntCrash) {
  ExitMenuRun();
  MenuController* controller = menu_controller();
  controller->Run(nullptr, nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
}

// Tests that if a MenuController is destroying during drag/drop, and another
// MenuController becomes active, that the exiting of drag does not cause a
// crash.
TEST_F(MenuControllerTest, MenuControllerReplacedDuringDrag) {
  // Build the menu so that the appropriate root window is available to set the
  // drag drop client on.
  AddButtonMenuItems(/*single_child=*/false);
  TestDragDropClient drag_drop_client(base::BindRepeating(
      &MenuControllerTest::TestMenuControllerReplacementDuringDrag,
      base::Unretained(this)));
  aura::client::SetDragDropClient(
      GetRootWindow(menu_item()->GetSubmenu()->GetWidget()), &drag_drop_client);
  StartDrag();
}

// Tests that if a CancelAll is called during drag-and-drop that it does not
// destroy the MenuController. On Windows and Linux this destruction also
// destroys the Widget used for drag-and-drop, thereby ending the drag.
TEST_F(MenuControllerTest, CancelAllDuringDrag) {
  // Build the menu so that the appropriate root window is available to set the
  // drag drop client on.
  AddButtonMenuItems(/*single_child=*/false);
  TestDragDropClient drag_drop_client(base::BindRepeating(
      &MenuControllerTest::TestCancelAllDuringDrag, base::Unretained(this)));
  aura::client::SetDragDropClient(
      GetRootWindow(menu_item()->GetSubmenu()->GetWidget()), &drag_drop_client);
  StartDrag();
}

// Tests that when releasing the ref on ViewsDelegate and MenuController is
// deleted, that shutdown occurs without crashing.
TEST_F(MenuControllerTest, DestroyedDuringViewsRelease) {
  ExitMenuRun();
  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  TestDestroyedDuringViewsRelease();
}

// Tests that when a context menu is opened above an empty menu item, and a
// right-click occurs over the empty item, that the bottom menu is not hidden,
// that a request to relaunch the context menu is received, and that
// subsequently pressing ESC does not crash the browser.
TEST_F(MenuControllerTest, RepostEventToEmptyMenuItem) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  // Setup a submenu. Additionally hook up appropriate Widget and View
  // containers, with bounds, so that hit testing works.
  MenuController* controller = menu_controller();
  MenuItemView* base_menu = menu_item();
  base_menu->SetBounds(0, 0, 200, 200);
  SubmenuView* base_submenu = base_menu->GetSubmenu();
  base_submenu->SetBounds(0, 0, 200, 200);
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.do_capture = false;
  base_submenu->ShowAt(params);
  GetMenuHost(base_submenu)
      ->SetContentsView(base_submenu->GetScrollViewContainer());

  // Build the submenu to have an empty menu item. Additionally hook up
  // appropriate Widget and View containers with bounds, so that hit testing
  // works.
  std::unique_ptr<TestMenuDelegate> sub_menu_item_delegate =
      std::make_unique<TestMenuDelegate>();
  std::unique_ptr<TestMenuItemViewShown> sub_menu_item =
      std::make_unique<TestMenuItemViewShown>(sub_menu_item_delegate.get());
  sub_menu_item->AddEmptyMenusForTest();
  sub_menu_item->SetController(controller);
  sub_menu_item->SetBounds(0, 50, 50, 50);
  base_submenu->AddChildView(sub_menu_item.get());
  SubmenuView* sub_menu_view = sub_menu_item->GetSubmenu();
  sub_menu_view->SetBounds(0, 50, 50, 50);
  params.parent = owner();
  params.bounds = gfx::Rect(0, 50, 50, 50);
  params.do_capture = false;
  sub_menu_view->ShowAt(params);
  GetMenuHost(sub_menu_view)
      ->SetContentsView(sub_menu_view->GetScrollViewContainer());

  // Set that the last selection target was the item which launches the submenu,
  // as the empty item can never become a target.
  SetPendingStateItem(sub_menu_item.get());

  // Nest a context menu.
  std::unique_ptr<TestMenuDelegate> nested_menu_delegate_1 =
      std::make_unique<TestMenuDelegate>();
  std::unique_ptr<TestMenuItemViewShown> nested_menu_item_1 =
      std::make_unique<TestMenuItemViewShown>(nested_menu_delegate_1.get());
  nested_menu_item_1->SetBounds(0, 0, 100, 100);
  nested_menu_item_1->SetController(controller);
  std::unique_ptr<TestMenuControllerDelegate> nested_controller_delegate_1 =
      std::make_unique<TestMenuControllerDelegate>();
  controller->AddNestedDelegate(nested_controller_delegate_1.get());
  controller->Run(owner(), nullptr, nested_menu_item_1.get(),
                  gfx::Rect(150, 50, 100, 100), MenuAnchorPosition::kTopLeft,
                  true, false);

  SubmenuView* nested_menu_submenu = nested_menu_item_1->GetSubmenu();
  nested_menu_submenu->SetBounds(0, 0, 100, 100);
  params.parent = owner();
  params.bounds = gfx::Rect(0, 0, 100, 100);
  params.do_capture = false;
  nested_menu_submenu->ShowAt(params);
  GetMenuHost(nested_menu_submenu)
      ->SetContentsView(nested_menu_submenu->GetScrollViewContainer());

  // Press down outside of the context menu, and within the empty menu item.
  // This should close the first context menu.
  gfx::Point press_location(sub_menu_view->bounds().CenterPoint());
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, press_location,
                             press_location, ui::EventTimeForNow(),
                             ui::EF_RIGHT_MOUSE_BUTTON, 0);
  ProcessMousePressed(nested_menu_submenu, press_event);
  EXPECT_EQ(nested_controller_delegate_1->on_menu_closed_called(), 1);
  EXPECT_EQ(menu_controller_delegate(), GetCurrentDelegate());

  // While the current state is the menu item which launched the sub menu, cause
  // a drag in the empty menu item. This should not hide the menu.
  SetState(sub_menu_item.get());
  press_location.Offset(-5, 0);
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, press_location,
                            press_location, ui::EventTimeForNow(),
                            ui::EF_RIGHT_MOUSE_BUTTON, 0);
  ProcessMouseDragged(sub_menu_view, drag_event);
  EXPECT_EQ(menu_delegate()->will_hide_menu_count(), 0);

  // Release the mouse in the empty menu item, triggering a context menu
  // request.
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, press_location,
                               press_location, ui::EventTimeForNow(),
                               ui::EF_RIGHT_MOUSE_BUTTON, 0);
  ProcessMouseReleased(sub_menu_view, release_event);
  EXPECT_EQ(sub_menu_item_delegate->show_context_menu_count(), 1);
  EXPECT_EQ(sub_menu_item_delegate->show_context_menu_source(),
            sub_menu_item.get());

  // Nest a context menu.
  std::unique_ptr<TestMenuDelegate> nested_menu_delegate_2 =
      std::make_unique<TestMenuDelegate>();
  std::unique_ptr<TestMenuItemViewShown> nested_menu_item_2 =
      std::make_unique<TestMenuItemViewShown>(nested_menu_delegate_2.get());
  nested_menu_item_2->SetBounds(0, 0, 100, 100);
  nested_menu_item_2->SetController(controller);

  std::unique_ptr<TestMenuControllerDelegate> nested_controller_delegate_2 =
      std::make_unique<TestMenuControllerDelegate>();
  controller->AddNestedDelegate(nested_controller_delegate_2.get());
  controller->Run(owner(), nullptr, nested_menu_item_2.get(),
                  gfx::Rect(150, 50, 100, 100), MenuAnchorPosition::kTopLeft,
                  true, false);

  // The escape key should only close the nested menu. SelectByChar should not
  // crash.
  TestAsyncEscapeKey();
  EXPECT_EQ(nested_controller_delegate_2->on_menu_closed_called(), 1);
  EXPECT_EQ(menu_controller_delegate(), GetCurrentDelegate());
}

// Drag the mouse from an external view into a menu
// When the mouse leaves the menu while still in the process of dragging
// the menu item view highlight should turn off
TEST_F(MenuControllerTest, DragFromViewIntoMenuAndExit) {
  SubmenuView* sub_menu = menu_item()->GetSubmenu();
  MenuItemView* first_item = sub_menu->GetMenuItemAt(0);

  std::unique_ptr<View> drag_view = std::make_unique<View>();
  drag_view->SetBoundsRect(gfx::Rect(0, 500, 100, 100));
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = gfx::Rect(0, 0, 100, 100);
  params.do_capture = false;
  sub_menu->ShowAt(params);
  gfx::Point press_location(drag_view->bounds().CenterPoint());
  gfx::Point drag_location(first_item->bounds().CenterPoint());
  gfx::Point release_location(200, 50);

  // Begin drag on an external view
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, press_location,
                             press_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  drag_view->OnMousePressed(press_event);

  // Drag into a menu item
  ui::MouseEvent drag_event_enter(ui::ET_MOUSE_DRAGGED, drag_location,
                                  drag_location, ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  ProcessMouseDragged(sub_menu, drag_event_enter);
  EXPECT_TRUE(first_item->IsSelected());

  // Drag out of the menu item
  ui::MouseEvent drag_event_exit(ui::ET_MOUSE_DRAGGED, release_location,
                                 release_location, ui::EventTimeForNow(),
                                 ui::EF_LEFT_MOUSE_BUTTON, 0);
  ProcessMouseDragged(sub_menu, drag_event_exit);
  EXPECT_FALSE(first_item->IsSelected());

  // Complete drag with release
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, release_location,
                               release_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  ProcessMouseReleased(sub_menu, release_event);
}

// Tests that |MenuHost::InitParams| are correctly forwarded to the created
// |aura::Window|.
TEST_F(MenuControllerTest, AuraWindowIsInitializedWithMenuHostInitParams) {
  SubmenuView* sub_menu = menu_item()->GetSubmenu();

  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = gfx::Rect(0, 0, 100, 100);
  params.do_capture = false;
  params.menu_type = ui::MenuType::kRootMenu;
  sub_menu->ShowAt(params);

  aura::Window* window = sub_menu->GetWidget()->GetNativeWindow();
  EXPECT_EQ(ui::MenuType::kRootMenu,
            window->GetProperty(aura::client::kMenuType));
}

// Tests that |aura::Window| has the correct properties when a context menu is
// shown.
TEST_F(MenuControllerTest, ContextMenuInitializesAuraWindowWhenShown) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  SubmenuView* sub_menu = menu_item()->GetSubmenu();

  // Checking that context menu properties are calculated correctly.
  menu_controller()->Run(owner(), nullptr, menu_item(), menu_item()->bounds(),
                         MenuAnchorPosition::kTopLeft, true, false);
  OpenMenu(menu_item());

  aura::Window* window = sub_menu->GetWidget()->GetNativeWindow();
  EXPECT_EQ(ui::MenuType::kRootContextMenu,
            window->GetProperty(aura::client::kMenuType));
  ui::OwnedWindowAnchor* anchor =
      window->GetProperty(aura::client::kOwnedWindowAnchor);
  EXPECT_TRUE(anchor);
  EXPECT_EQ(ui::OwnedWindowAnchorPosition::kBottomLeft,
            anchor->anchor_position);
  EXPECT_EQ(ui::OwnedWindowAnchorGravity::kBottomRight, anchor->anchor_gravity);
  EXPECT_EQ((ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipY |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentRezizeY),
            anchor->constraint_adjustment);
  EXPECT_EQ(
      CalculateExpectedMenuAnchorRect(menu_item(), window->GetBoundsInScreen()),
      anchor->anchor_rect);

  // Checking that child menu properties are calculated correctly.
  MenuItemView* const child_menu = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  child_menu->CreateSubmenu();
  ASSERT_NE(nullptr, child_menu->GetParentMenuItem());
  menu_controller()->Run(owner(), nullptr, child_menu, child_menu->bounds(),
                         MenuAnchorPosition::kTopRight, false, false);
  OpenMenu(child_menu);

  ASSERT_NE(nullptr, child_menu->GetWidget());
  window = child_menu->GetSubmenu()->GetWidget()->GetNativeWindow();

  anchor = window->GetProperty(aura::client::kOwnedWindowAnchor);
  EXPECT_TRUE(anchor);
  EXPECT_EQ(ui::MenuType::kChildMenu,
            window->GetProperty(aura::client::kMenuType));
  EXPECT_EQ(ui::OwnedWindowAnchorPosition::kTopRight, anchor->anchor_position);
  EXPECT_EQ(ui::OwnedWindowAnchorGravity::kBottomRight, anchor->anchor_gravity);
  EXPECT_EQ((ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideY |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentRezizeY),
            anchor->constraint_adjustment);
  EXPECT_EQ(
      CalculateExpectedMenuAnchorRect(child_menu, window->GetBoundsInScreen()),
      anchor->anchor_rect);
}

// Tests that |aura::Window| has the correct properties when a root or a child
// menu is shown.
TEST_F(MenuControllerTest, RootAndChildMenusInitializeAuraWindowWhenShown) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  SubmenuView* sub_menu = menu_item()->GetSubmenu();

  // Checking that root menu properties are calculated correctly.
  menu_controller()->Run(owner(), nullptr, menu_item(), menu_item()->bounds(),
                         MenuAnchorPosition::kTopLeft, false, false);
  OpenMenu(menu_item());

  aura::Window* window = sub_menu->GetWidget()->GetNativeWindow();
  ui::OwnedWindowAnchor* anchor =
      window->GetProperty(aura::client::kOwnedWindowAnchor);
  EXPECT_TRUE(anchor);
  EXPECT_EQ(ui::MenuType::kRootMenu,
            window->GetProperty(aura::client::kMenuType));
  EXPECT_EQ(ui::OwnedWindowAnchorPosition::kBottomLeft,
            anchor->anchor_position);
  EXPECT_EQ(ui::OwnedWindowAnchorGravity::kBottomRight, anchor->anchor_gravity);
  EXPECT_EQ((ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipY |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentRezizeY),
            anchor->constraint_adjustment);
  EXPECT_EQ(
      CalculateExpectedMenuAnchorRect(menu_item(), window->GetBoundsInScreen()),
      anchor->anchor_rect);

  // Checking that child menu properties are calculated correctly.
  MenuItemView* const child_menu = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  child_menu->CreateSubmenu();
  ASSERT_NE(nullptr, child_menu->GetParentMenuItem());
  menu_controller()->Run(owner(), nullptr, child_menu, child_menu->bounds(),
                         MenuAnchorPosition::kTopRight, false, false);
  OpenMenu(child_menu);

  ASSERT_NE(nullptr, child_menu->GetWidget());
  window = child_menu->GetSubmenu()->GetWidget()->GetNativeWindow();

  anchor = window->GetProperty(aura::client::kOwnedWindowAnchor);
  EXPECT_TRUE(anchor);
  EXPECT_EQ(ui::MenuType::kChildMenu,
            window->GetProperty(aura::client::kMenuType));
  EXPECT_EQ(ui::OwnedWindowAnchorPosition::kTopRight, anchor->anchor_position);
  EXPECT_EQ(ui::OwnedWindowAnchorGravity::kBottomRight, anchor->anchor_gravity);
  EXPECT_EQ((ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideY |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentRezizeY),
            anchor->constraint_adjustment);
  auto anchor_rect = anchor->anchor_rect;
  EXPECT_EQ(
      CalculateExpectedMenuAnchorRect(child_menu, window->GetBoundsInScreen()),
      anchor->anchor_rect);

  // Try to reposition the existing menu. Its anchor must change.
  child_menu->SetY(menu_item()->bounds().y() + 2);
  menu_controller()->Run(owner(), nullptr, child_menu, child_menu->bounds(),
                         MenuAnchorPosition::kTopLeft, false, false);
  MenuChildrenChanged(child_menu);

  EXPECT_EQ(
      CalculateExpectedMenuAnchorRect(child_menu, window->GetBoundsInScreen()),
      anchor->anchor_rect);
  // New anchor mustn't be the same as the old one.
  EXPECT_NE(anchor->anchor_rect, anchor_rect);
}

#endif  // defined(USE_AURA)

// Tests that having the MenuController deleted during OnMousePressed does not
// cause a crash. ASAN bots should not detect use-after-free in MenuController.
TEST_F(MenuControllerTest, NoUseAfterFreeWhenMenuCanceledOnMousePress) {
  MenuController* controller = menu_controller();
  DestroyMenuControllerOnMenuClosed(menu_controller_delegate());

  // Creating own MenuItem for a minimal test environment.
  auto item = std::make_unique<TestMenuItemViewNotShown>(menu_delegate());
  item->SetController(controller);
  item->SetBounds(0, 0, 50, 50);

  SubmenuView* sub_menu = item->CreateSubmenu();
  auto* canceling_view = new CancelMenuOnMousePressView(controller);
  sub_menu->AddChildView(canceling_view);
  canceling_view->SetBoundsRect(item->bounds());

  controller->Run(owner(), nullptr, item.get(), item->bounds(),
                  MenuAnchorPosition::kTopLeft, false, false);
  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = item->bounds();
  params.do_capture = true;
  sub_menu->ShowAt(params);

  // Simulate a mouse press in the middle of the |closing_widget|.
  gfx::Point location(canceling_view->bounds().CenterPoint());
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  EXPECT_TRUE(controller->OnMousePressed(sub_menu, event));

  // Close to remove observers before test TearDown.
  sub_menu->Close();
}

TEST_F(MenuControllerTest, SetSelectionIndices_MenuItemsOnly) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest,
       SetSelectionIndices_MenuItemsOnly_SkipHiddenAndDisabled) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  item1->SetEnabled(false);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  item3->SetVisible(false);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  OpenMenu(menu_item());

  ui::AXNodeData data;
  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest, SetSelectionIndices_Buttons) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  AddButtonMenuItems(/*single_child=*/false);
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  MenuItemView* const item5 = menu_item()->GetSubmenu()->GetMenuItemAt(4);
  Button* const button1 = Button::AsButton(item5->children()[0]);
  Button* const button2 = Button::AsButton(item5->children()[1]);
  Button* const button3 = Button::AsButton(item5->children()[2]);
  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  button1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  button2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(6, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  button3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest, SetSelectionIndices_Buttons_SkipHiddenAndDisabled) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  AddButtonMenuItems(/*single_child=*/false);
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  MenuItemView* const item5 = menu_item()->GetSubmenu()->GetMenuItemAt(4);
  Button* const button1 = Button::AsButton(item5->children()[0]);
  button1->SetEnabled(false);
  Button* const button2 = Button::AsButton(item5->children()[1]);
  button2->SetVisible(false);
  Button* const button3 = Button::AsButton(item5->children()[2]);
  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  button3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest, SetSelectionIndices_NestedButtons) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);

  // This simulates how buttons are nested in views in the main app menu.
  View* const container_view = new View();
  container_view->GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenu);
  item4->AddChildView(container_view);

  // There's usually a label before the traversable elements.
  container_view->AddChildView(new Label());

  // Add two focusable buttons (buttons in menus are always focusable).
  Button* const button1 =
      container_view->AddChildView(std::make_unique<LabelButton>());
  button1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button1->GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenuItem);
  Button* const button2 =
      container_view->AddChildView(std::make_unique<LabelButton>());
  button2->GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenuItem);
  button2->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  data = ui::AXNodeData();
  button1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  data = ui::AXNodeData();
  button2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest, SetSelectionIndices_ChildrenChanged) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  AddButtonMenuItems(/*single_child=*/false);
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  MenuItemView* const item5 = menu_item()->GetSubmenu()->GetMenuItemAt(4);
  Button* const button1 = Button::AsButton(item5->children()[0]);
  Button* const button2 = Button::AsButton(item5->children()[1]);
  Button* const button3 = Button::AsButton(item5->children()[2]);
  OpenMenu(menu_item());

  auto expect_coordinates = [](View* v, absl::optional<int> pos,
                               absl::optional<int> size) {
    ui::AXNodeData data;
    v->GetViewAccessibility().GetAccessibleNodeData(&data);
    if (pos.has_value()) {
      EXPECT_EQ(pos.value(),
                data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
    } else {
      EXPECT_FALSE(data.HasIntAttribute(ax::mojom::IntAttribute::kPosInSet));
    }
    if (size.has_value()) {
      EXPECT_EQ(size.value(),
                data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
    } else {
      EXPECT_FALSE(data.HasIntAttribute(ax::mojom::IntAttribute::kSetSize));
    }
  };

  expect_coordinates(item1, 1, 7);
  expect_coordinates(item2, 2, 7);
  expect_coordinates(item3, 3, 7);
  expect_coordinates(item4, 4, 7);
  expect_coordinates(button1, 5, 7);
  expect_coordinates(button2, 6, 7);
  expect_coordinates(button3, 7, 7);

  // Simulate a menu model update.
  item1->SetEnabled(false);
  button1->SetEnabled(false);
  MenuItemView* item6 = menu_item()->AppendMenuItem(6, u"Six");
  menu_item()->RemoveMenuItem(item2);
  item2 = nullptr;
  MenuChildrenChanged(menu_item());

  // Verify that disabled menu items no longer have PosInSet or SetSize.
  expect_coordinates(item1, absl::nullopt, absl::nullopt);
  expect_coordinates(button1, absl::nullopt, absl::nullopt);
  expect_coordinates(item3, 1, 5);
  expect_coordinates(item4, 2, 5);
  expect_coordinates(button2, 3, 5);
  expect_coordinates(button3, 4, 5);
  expect_coordinates(item6, 5, 5);
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when accessibility performs a do default action.
TEST_F(MenuControllerTest, AccessibilityDoDefaultCallsAccept) {
  views::test::DisableMenuClosureAnimations();

  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  MenuItemView* accepted = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  accepted->HandleAccessibleAction(data);
  views::test::WaitForMenuClosureAnimation();

  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(accepted, delegate->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
}

// Test that the kSelectedChildrenChanged event is emitted on
// the root menu item when the selected menu item changes.
TEST_F(MenuControllerTest, AccessibilityEmitsSelectChildrenChanged) {
  AXEventCounter ax_counter(views::AXEventManager::Get());
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  // Arrow down to select an item checking the event has been emitted.
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged), 0);
  DispatchKey(ui::VKEY_DOWN);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged), 1);

  DispatchKey(ui::VKEY_DOWN);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged), 2);
}

// Test that in accessibility mode disabled menu items are taken into account
// during items indices assignment.
TEST_F(MenuControllerTest, AccessibilityDisabledItemsIndices) {
  ScopedAXModeSetter ax_mode_setter_{ui::AXMode::kNativeAPIs};

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1286137): This test is consistently failing on Win11.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Skipping test for WIN11_21H2 and greater";
  }
#endif
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);

  item2->SetEnabled(false);

  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

#if BUILDFLAG(IS_MAC)
// This test exercises a Mac-specific behavior, by which hotkeys using modifiers
// cause menus to close and the hotkeys to be handled by the browser window.
// This specific test case tries using cmd-ctrl-f, which normally means
// "Fullscreen".
TEST_F(MenuControllerTest, BrowserHotkeysCancelMenusAndAreRedispatched) {
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  int options = ui::EF_COMMAND_DOWN;
  ui::KeyEvent press_cmd(ui::ET_KEY_PRESSED, ui::VKEY_COMMAND, options);
  menu_controller()->OnWillDispatchKeyEvent(&press_cmd);
  EXPECT_TRUE(IsShowing());  // ensure the command press itself doesn't cancel

  options |= ui::EF_CONTROL_DOWN;
  ui::KeyEvent press_ctrl(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, options);
  menu_controller()->OnWillDispatchKeyEvent(&press_ctrl);
  EXPECT_TRUE(IsShowing());

  ui::KeyEvent press_f(ui::ET_KEY_PRESSED, ui::VKEY_F, options);
  menu_controller()->OnWillDispatchKeyEvent(&press_f);  // to pay respects
  EXPECT_FALSE(IsShowing());
  EXPECT_FALSE(press_f.handled());
  EXPECT_FALSE(press_f.stopped_propagation());
}
#endif

class ExecuteCommandWithoutClosingMenuTest : public MenuControllerTest {
 public:
  void SetUp() override {
    MenuControllerTest::SetUp();

    views::test::DisableMenuClosureAnimations();
    menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                           MenuAnchorPosition::kTopLeft, false, false);

    MenuHost::InitParams params;
    params.parent = owner();
    params.bounds = gfx::Rect(0, 0, 100, 100);
    params.do_capture = false;
    menu_item()->GetSubmenu()->ShowAt(params);

    menu_delegate()->set_should_execute_command_without_closing_menu(true);
  }
};

TEST_F(ExecuteCommandWithoutClosingMenuTest, OnClick) {
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  MenuItemView* menu_item_view = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  gfx::Point press_location(menu_item_view->bounds().CenterPoint());
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, press_location,
                             press_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, press_location,
                               press_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  ProcessMousePressed(menu_item()->GetSubmenu(), press_event);
  ProcessMouseReleased(menu_item()->GetSubmenu(), release_event);

  EXPECT_EQ(0, delegate->on_menu_closed_called());
  EXPECT_TRUE(IsShowing());
  EXPECT_EQ(menu_delegate()->execute_command_id(),
            menu_item_view->GetCommand());
}

TEST_F(ExecuteCommandWithoutClosingMenuTest, OnTap) {
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  MenuItemView* menu_item_view = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  gfx::Point tap_location(menu_item_view->bounds().CenterPoint());
  ui::GestureEvent event(tap_location.x(), tap_location.y(), 0,
                         ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  ProcessGestureEvent(menu_item()->GetSubmenu(), event);

  EXPECT_EQ(0, delegate->on_menu_closed_called());
  EXPECT_TRUE(IsShowing());
  EXPECT_EQ(menu_delegate()->execute_command_id(),
            menu_item_view->GetCommand());
}

TEST_F(ExecuteCommandWithoutClosingMenuTest, OnReturnKey) {
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  DispatchKey(ui::VKEY_DOWN);
  DispatchKey(ui::VKEY_RETURN);

  EXPECT_EQ(0, delegate->on_menu_closed_called());
  EXPECT_TRUE(IsShowing());
  EXPECT_EQ(menu_delegate()->execute_command_id(),
            menu_item()->GetSubmenu()->GetMenuItemAt(0)->GetCommand());
}

}  // namespace views::test
