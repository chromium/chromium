// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ozone_buildflags.h"
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
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_OZONE_X11)
#include "ui/events/test/events_test_utils_x11.h"
#endif

namespace views {
namespace {

using ::ui::mojom::DragOperation;

constexpr MenuAnchorPosition kBubblePositions[] = {
    MenuAnchorPosition::kBubbleTopLeft,
    MenuAnchorPosition::kBubbleTopRight,
    MenuAnchorPosition::kBubbleLeft,
    MenuAnchorPosition::kBubbleRight,
    MenuAnchorPosition::kBubbleBottomLeft,
    MenuAnchorPosition::kBubbleBottomRight};

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

gfx::Size GetPreferredSizeForSubmenu(SubmenuView& submenu) {
  auto size = submenu.GetPreferredSize({});
  const auto insets = submenu.GetScrollViewContainer()->GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

// Unfortunately a macro rather than a function, to work around ASSERT_ not
// working in non-void functions.
#define GET_CHILD_BUTTON(name, parent, index)                       \
  ASSERT_GT((parent)->children().size(), size_t{(index)});          \
  auto* const name = Button::AsButton((parent)->children()[index]); \
  ASSERT_NE(nullptr, name)

// Test implementation of MenuControllerDelegate that only reports the values
// called of OnMenuClosed.
class TestMenuControllerDelegate : public internal::MenuControllerDelegate {
 public:
  TestMenuControllerDelegate() = default;

  int on_menu_closed_called() const { return on_menu_closed_called_; }

  NotifyType on_menu_closed_notify_type() const {
    return on_menu_closed_notify_type_;
  }

  const MenuItemView* on_menu_closed_menu() const {
    return on_menu_closed_menu_;
  }

  int on_menu_closed_mouse_event_flags() const {
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

void TestMenuControllerDelegate::OnMenuClosed(NotifyType type,
                                              MenuItemView* menu,
                                              int mouse_event_flags) {
  ++on_menu_closed_called_;
  on_menu_closed_notify_type_ = type;
  on_menu_closed_menu_ = menu;
  on_menu_closed_mouse_event_flags_ = mouse_event_flags;
  if (on_menu_closed_callback_) {
    on_menu_closed_callback_.Run();
  }
}

void TestMenuControllerDelegate::SiblingMenuCreated(MenuItemView* menu) {}

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() = default;

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (event->type() == ui::EventType::kTouchPressed) {
      ++outstanding_touches_;
    } else if (event->type() == ui::EventType::kTouchReleased ||
               event->type() == ui::EventType::kTouchCancelled) {
      --outstanding_touches_;
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

  void OnGestureEvent(ui::GestureEvent* event) override;

  int gesture_count() const { return gesture_count_; }

 private:
  int gesture_count_ = 0;
};

void GestureTestWidget::OnGestureEvent(ui::GestureEvent* event) {
  ++gesture_count_;
}

#if defined(USE_AURA)
// A DragDropClient which does not trigger a nested run loop. Instead a
// callback is triggered during StartDragAndDrop in order to allow testing.
class TestDragDropClient : public aura::client::DragDropClient {
 public:
  explicit TestDragDropClient(base::RepeatingClosure callback)
      : start_drag_and_drop_callback_(std::move(callback)) {}

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
  METADATA_HEADER(CancelMenuOnMousePressView, View)

 public:
  explicit CancelMenuOnMousePressView(base::WeakPtr<MenuController> controller)
      : controller_(controller) {}

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;

 private:
  const base::WeakPtr<MenuController> controller_;
};

bool CancelMenuOnMousePressView::OnMousePressed(const ui::MouseEvent& event) {
  controller_->Cancel(MenuController::ExitType::kAll);
  return true;
}

gfx::Size CancelMenuOnMousePressView::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  // This is needed to prevent the view from being "squashed" to zero height
  // when the menu which owns it is shown. In such state the logic which
  // determines if the menu contains the mouse press location doesn't work.
  return size();
}

BEGIN_METADATA(CancelMenuOnMousePressView)
END_METADATA

}  // namespace

struct MenuBoundsOptions {
  gfx::Rect anchor_bounds = gfx::Rect(500, 500, 10, 10);
  gfx::Rect monitor_bounds = gfx::Rect(0, 0, 1000, 1000);
  gfx::Size menu_size = gfx::Size(100, 150);
  MenuAnchorPosition menu_anchor = MenuAnchorPosition::kTopLeft;
  MenuItemView::MenuPosition menu_position =
      MenuItemView::MenuPosition::kBestFit;
};

class MenuControllerTest : public ViewsTestBase,
                           public testing::WithParamInterface<bool> {
 public:
  MenuControllerTest() = default;

  // ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

  void ReleaseTouchId(int id);

  void PressKey(ui::KeyboardCode key_code);

  void DispatchKey(ui::KeyboardCode key_code);

  gfx::Rect CalculateMenuBounds(const MenuBoundsOptions& options);

  gfx::Rect CalculateBubbleMenuBoundsWithoutInsets(
      const MenuBoundsOptions& options,
      MenuItemView* menu_item = nullptr);

  MenuItemView::MenuPosition menu_item_actual_position() const {
    return menu_item_->actual_menu_position();
  }

  gfx::Rect CalculateExpectedMenuAnchorRect(MenuItemView* menu_item);

  MenuController::MenuOpenDirection GetChildMenuOpenDirectionAtDepth(
      size_t depth) const;

  void SetChildMenuOpenDirectionAtDepth(
      size_t depth,
      MenuController::MenuOpenDirection direction);

  void MenuChildrenChanged(MenuItemView* item);

  static MenuAnchorPosition AdjustAnchorPositionForRtl(
      MenuAnchorPosition position);

#if defined(USE_AURA)
  // Verifies that a non-nested menu fully closes when receiving an escape key.
  void TestAsyncEscapeKey();

  // Verifies that an open menu receives a cancel event, and closes.
  void TestCancelEvent();
#endif  // defined(USE_AURA)

  // Verifies the state of the |menu_controller_| before destroying it.
  void VerifyDragCompleteThenDestroy();

  // Sets up |menu_controller_delegate_| to be destroyed when OnMenuClosed is
  // called.
  void TestDragCompleteThenDestroyOnMenuClosed();

  // Tests destroying the active |menu_controller_| and replacing it with a new
  // active instance.
  void TestMenuControllerReplacementDuringDrag();

  // Tests that the menu does not destroy itself when canceled during a drag.
  void TestCancelAllDuringDrag();

  // Tests that destroying the menu during ViewsDelegate::ReleaseRef does not
  // cause a crash.
  void TestDestroyedDuringViewsRelease();

  void TestMenuFitsOnScreen(MenuAnchorPosition menu_anchor_position,
                            const gfx::Rect& monitor_bounds);

  void TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition menu_anchor_position,
                                       const gfx::Rect& monitor_bounds);

  void TestMenuFitsOnSmallScreen(MenuAnchorPosition menu_anchor_position,
                                 const gfx::Rect& monitor_bounds);

  // Given an onscreen menu `item` with screen bounds `parent_bounds`, verifies
  // that the submenu opened from `item` fits inside `monitor_bounds`.
  void TestSubmenuFitsOnScreen(MenuItemView* item,
                               const gfx::Rect& monitor_bounds,
                               const gfx::Rect& parent_bounds,
                               MenuAnchorPosition menu_anchor);

 protected:
  void SetPendingStateItem(MenuItemView* item);

  void SetState(MenuItemView* item);

  void IncrementSelection();

  void DecrementSelection();

  void DestroyMenuControllerOnMenuClosed(TestMenuControllerDelegate* delegate);

  MenuItemView* FindInitialSelectableMenuItemDown(MenuItemView* parent);

  MenuItemView* FindInitialSelectableMenuItemUp(MenuItemView* parent);

  internal::MenuControllerDelegate* current_controller_delegate() {
    return menu_controller_->delegate_;
  }

  bool showing() const { return menu_controller_->showing_; }

  MenuHost* menu_host_for_submenu(SubmenuView* submenu) {
    return submenu->host_;
  }

  MenuHostRootView* CreateMenuHostRootView(MenuHost* host);

  void MenuHostOnDragWillStart(MenuHost* host);

  void MenuHostOnDragComplete(MenuHost* host);

  void SelectByChar(char16_t character);

  void SetDropMenuItem(MenuItemView* target,
                       MenuDelegate::DropPosition position);

  void SetComboboxType(MenuController::ComboboxType combobox_type);

  template <typename T>
  static T ConvertEvent(View* source, const T& event) {
    return T(event, source, source->GetWidget()->GetRootView());
  }

  // These functions expect `event` to be in coordinates of `source`.
  void SetSelectionOnPointerDown(SubmenuView* source,
                                 const ui::MouseEvent& event);
  bool ProcessMousePressed(SubmenuView* source, const ui::MouseEvent& event);
  bool ProcessMouseDragged(SubmenuView* source, const ui::MouseEvent& event);
  void ProcessMouseReleased(SubmenuView* source, const ui::MouseEvent& event);
  void ProcessMouseMoved(SubmenuView* source, const ui::MouseEvent& event);
  void ProcessGestureEvent(SubmenuView* source, const ui::GestureEvent& event);
  void ProcessTouchEvent(SubmenuView* source, const ui::TouchEvent& event);

  void Accept(MenuItemView* item, int event_flags);

  // Causes the |menu_controller_| to begin dragging. Use TestDragDropClient to
  // avoid nesting message loops.
  void StartDrag();

  void SetUpMenuControllerForCalculateBounds(const MenuBoundsOptions& options,
                                             MenuItemView* menu_item);

  GestureTestWidget* owner() { return owner_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  MenuItemView* menu_item() { return menu_item_.get(); }
  test::TestMenuDelegate* menu_delegate() { return menu_delegate_.get(); }
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

  // Displays a submenu with appropriate params. If the first arg is null (the
  // default), displays `menu_item()->GetSubmenu()`. Supply a second arg if you
  // want a callback to modify the init params before calling
  // `SubmenuView::ShowAt()`.
  template <typename T = void (*)(MenuHost::InitParams&),
            typename =
                std::enable_if_t<std::is_invocable_v<T, MenuHost::InitParams&>>>
  void ShowSubmenu(
      SubmenuView* submenu = nullptr,
      T&& adjust_params = [](auto&) {}) {
    if (!submenu) {
      submenu = menu_item()->GetSubmenu();
    }
    MenuHost::InitParams params;
    params.parent = owner();
    params.bounds = gfx::Rect(GetPreferredSizeForSubmenu(*submenu));
    std::invoke(std::forward<T>(adjust_params), params);
    submenu->ShowAt(params);
  }

  // Adds a menu item having buttons as children and returns it. If
  // `single_child` is true, the hosting menu item has only one child button.
  MenuItemView* AddButtonMenuItems(bool single_child);

  void DestroyMenuItem();

  Button* hot_button() { return menu_controller_->hot_button_; }

  void SetHotTrackedButton(Button* hot_button);

  void ExitMenuRun();

  void DestroyMenuController();

  void DestroyMenuControllerDelegate();

  int owner_gesture_count() const { return owner_->gesture_count(); }

  static bool SelectionWraps();

  void OpenMenu(MenuItemView* parent,
                const MenuBoundsOptions& options = MenuBoundsOptions());

  gfx::Insets GetBorderAndShadowInsets(bool is_submenu);

 private:
  std::unique_ptr<GestureTestWidget> owner_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<MenuItemView> menu_item_;
  std::unique_ptr<TestMenuControllerDelegate> menu_controller_delegate_;
  std::unique_ptr<test::TestMenuDelegate> menu_delegate_;
  raw_ptr<MenuController> menu_controller_ = nullptr;
};

void MenuControllerTest::SetUp() {
  if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
    base::i18n::SetRTLForTesting(GetParam());
  }

  set_views_delegate(std::make_unique<test::ReleaseRefTestViewsDelegate>());
  ViewsTestBase::SetUp();
  ASSERT_TRUE(base::CurrentUIThread::IsSet());

  owner_ = std::make_unique<GestureTestWidget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  owner_->Init(std::move(params));
  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(owner()));
  owner_->Show();

  menu_delegate_ = std::make_unique<test::TestMenuDelegate>();
  menu_item_ = std::make_unique<MenuItemView>(menu_delegate_.get());
  menu_item_->AppendMenuItem(1, u"One");
  menu_item_->AppendMenuItem(2, u"Two");
  menu_item_->AppendMenuItem(3, u"Three");
  menu_item_->AppendMenuItem(4, u"Four");

  menu_controller_delegate_ = std::make_unique<TestMenuControllerDelegate>();
  menu_controller_ =
      new MenuController(/*for_drop=*/false, menu_controller_delegate_.get());
  menu_controller_->owner_ = owner();
  menu_controller_->showing_ = true;
  menu_controller_->SetSelection(menu_item(),
                                 MenuController::SELECTION_UPDATE_IMMEDIATELY);
  menu_item_->set_controller(menu_controller_);
}

void MenuControllerTest::TearDown() {
  owner_->CloseNow();
  DestroyMenuController();
  // `menu_item_` must be torn down before `ViewsTestBase::TearDown()`, since
  // it may transitively own a `Compositor` that is registered with a context
  // factory that `TearDown()` will destroy.
  menu_item_.reset();
  ViewsTestBase::TearDown();
  base::i18n::SetRTLForTesting(false);
}

void MenuControllerTest::ReleaseTouchId(int id) {
  event_generator_->ReleaseTouchId(id);
}

void MenuControllerTest::PressKey(ui::KeyboardCode key_code) {
  event_generator_->PressKey(key_code, 0);
}

void MenuControllerTest::DispatchKey(ui::KeyboardCode key_code) {
  ui::KeyEvent event(ui::EventType::kKeyPressed, key_code, 0);
  menu_controller_->OnWillDispatchKeyEvent(&event);
}

gfx::Rect MenuControllerTest::CalculateMenuBounds(
    const MenuBoundsOptions& options) {
  SetUpMenuControllerForCalculateBounds(options, menu_item_.get());
  MenuController::MenuOpenDirection resulting_direction;
  ui::OwnedWindowAnchor anchor;
  return menu_controller_->CalculateMenuBounds(
      menu_item_.get(), MenuController::MenuOpenDirection::kLeading,
      &resulting_direction, &anchor);
}

gfx::Rect MenuControllerTest::CalculateBubbleMenuBoundsWithoutInsets(
    const MenuBoundsOptions& options,
    MenuItemView* menu_item) {
  if (!menu_item) {
    menu_item = menu_item_.get();
  }
  SetUpMenuControllerForCalculateBounds(options, menu_item);
  MenuController::MenuOpenDirection resulting_direction;
  ui::OwnedWindowAnchor anchor;
  gfx::Rect bounds = menu_controller_->CalculateBubbleMenuBounds(
      menu_item, MenuController::MenuOpenDirection::kLeading,
      &resulting_direction, &anchor);
  bounds.Inset(menu_item->GetSubmenu()
                   ->GetScrollViewContainer()
                   ->outside_border_insets());
  return bounds;
}

gfx::Rect MenuControllerTest::CalculateExpectedMenuAnchorRect(
    MenuItemView* menu_item) {
  if (!menu_item->GetParentMenuItem()) {
    return menu_controller_->state_.initial_bounds;
  }
  gfx::Rect bounds = menu_item->GetBoundsInScreen();
  bounds.set_height(1);
  return bounds;
}

MenuController::MenuOpenDirection
MenuControllerTest::GetChildMenuOpenDirectionAtDepth(size_t depth) const {
  return menu_controller_->GetChildMenuOpenDirectionAtDepth(depth);
}

void MenuControllerTest::SetChildMenuOpenDirectionAtDepth(
    size_t depth,
    MenuController::MenuOpenDirection direction) {
  menu_controller_->SetChildMenuOpenDirectionAtDepth(depth, direction);
}

void MenuControllerTest::MenuChildrenChanged(MenuItemView* item) {
  menu_controller_->MenuChildrenChanged(item);
}

// static
MenuAnchorPosition MenuControllerTest::AdjustAnchorPositionForRtl(
    MenuAnchorPosition position) {
  return MenuController::AdjustAnchorPositionForRtl(position);
}

#if defined(USE_AURA)
void MenuControllerTest::TestAsyncEscapeKey() {
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, 0);
  menu_controller_->OnWillDispatchKeyEvent(&event);
}

void MenuControllerTest::TestCancelEvent() {
  EXPECT_EQ(MenuController::ExitType::kNone, menu_controller_->exit_type());
  ui::CancelModeEvent cancel_event;
  event_generator_->Dispatch(&cancel_event);
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller_->exit_type());
}
#endif  // defined(USE_AURA)

void MenuControllerTest::VerifyDragCompleteThenDestroy() {
  EXPECT_FALSE(menu_controller()->drag_in_progress());
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
  DestroyMenuController();
}

void MenuControllerTest::TestDragCompleteThenDestroyOnMenuClosed() {
  menu_controller_delegate_->set_on_menu_closed_callback(
      base::BindRepeating(&MenuControllerTest::VerifyDragCompleteThenDestroy,
                          base::Unretained(this)));
}

void MenuControllerTest::TestMenuControllerReplacementDuringDrag() {
  DestroyMenuController();
  menu_item()->GetSubmenu()->Close();
  menu_controller_ =
      new MenuController(/*for_drop=*/false, menu_controller_delegate_.get());
  menu_controller_->owner_ = owner_.get();
  menu_controller_->showing_ = true;
}

void MenuControllerTest::TestCancelAllDuringDrag() {
  menu_controller_->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(0, menu_controller_delegate_->on_menu_closed_called());
}

void MenuControllerTest::TestDestroyedDuringViewsRelease() {
  // |test_views_delegate_| is owned by views::ViewsTestBase and not deleted
  // until TearDown. MenuControllerTest outlives it.
  static_cast<test::ReleaseRefTestViewsDelegate*>(test_views_delegate())
      ->set_release_ref_callback(base::BindRepeating(
          &MenuControllerTest::DestroyMenuController, base::Unretained(this)));
  menu_controller_->ExitMenu();
}

void MenuControllerTest::TestMenuFitsOnScreen(
    MenuAnchorPosition menu_anchor_position,
    const gfx::Rect& monitor_bounds) {
  constexpr int kButtonSize = 50;
  const auto test_within_bounds = [&](gfx::Point origin) {
    const MenuBoundsOptions options = {
        .anchor_bounds = gfx::Rect(origin, gfx::Size(kButtonSize, kButtonSize)),
        .monitor_bounds = monitor_bounds,
        .menu_anchor = menu_anchor_position};
    EXPECT_TRUE(options.monitor_bounds.Contains(
        CalculateBubbleMenuBoundsWithoutInsets(options)))
        << "Anchor position: " << base::ToString(menu_anchor_position)
        << ", monitor bounds: " << monitor_bounds.ToString()
        << ", origin: " << origin.ToString();
  };

  // Simulate a bottom shelf with a tall menu.
  const gfx::Point monitor_center = monitor_bounds.CenterPoint();
  test_within_bounds(
      gfx::Point(monitor_center.x(), monitor_bounds.bottom() - kButtonSize));

  // Simulate a left shelf with a tall menu.
  test_within_bounds(gfx::Point(monitor_bounds.x(), monitor_center.y()));

  // Simulate right shelf with a tall menu.
  test_within_bounds(
      gfx::Point(monitor_bounds.right() - kButtonSize, monitor_center.y()));
}

void MenuControllerTest::TestMenuFitsOnScreenSmallAnchor(
    MenuAnchorPosition menu_anchor_position,
    const gfx::Rect& monitor_bounds) {
  const auto test_within_bounds = [&](gfx::Point origin) {
    const MenuBoundsOptions options = {.anchor_bounds = gfx::Rect(origin, {}),
                                       .monitor_bounds = monitor_bounds,
                                       .menu_anchor = menu_anchor_position};
    EXPECT_TRUE(options.monitor_bounds.Contains(
        CalculateBubbleMenuBoundsWithoutInsets(options)))
        << "Anchor position: " << base::ToString(menu_anchor_position)
        << ", monitor bounds: " << monitor_bounds.ToString()
        << ", origin: " << origin.ToString();
  };
  test_within_bounds(monitor_bounds.origin());
  test_within_bounds(monitor_bounds.bottom_left());
  test_within_bounds(monitor_bounds.top_right());
  test_within_bounds(monitor_bounds.bottom_right());
}

void MenuControllerTest::TestMenuFitsOnSmallScreen(
    MenuAnchorPosition menu_anchor_position,
    const gfx::Rect& monitor_bounds) {
  const auto test_within_bounds = [&](gfx::Point origin) {
    const MenuBoundsOptions options = {
        .anchor_bounds = gfx::Rect(origin, {}),
        .monitor_bounds = monitor_bounds,
        .menu_size = monitor_bounds.size() + gfx::Size(100, 100),
        .menu_anchor = menu_anchor_position};
    EXPECT_TRUE(options.monitor_bounds.Contains(
        CalculateBubbleMenuBoundsWithoutInsets(options)))
        << "Anchor position: " << base::ToString(menu_anchor_position)
        << ", monitor bounds: " << monitor_bounds.ToString()
        << ", origin: " << origin.ToString();
  };
  test_within_bounds(monitor_bounds.origin());
  test_within_bounds(monitor_bounds.bottom_left());
  test_within_bounds(monitor_bounds.top_right());
  test_within_bounds(monitor_bounds.bottom_right());
  test_within_bounds(monitor_bounds.CenterPoint());
}

void MenuControllerTest::TestSubmenuFitsOnScreen(
    MenuItemView* item,
    const gfx::Rect& monitor_bounds,
    const gfx::Rect& parent_bounds,
    MenuAnchorPosition menu_anchor) {
  const MenuBoundsOptions options = {
      .monitor_bounds = monitor_bounds,
      .menu_size = GetPreferredSizeForSubmenu(*item->GetSubmenu()),
      .menu_anchor = menu_anchor};

  // Put the parent menu onscreen so that `item` is in a widget hierarchy and
  // `GetBoundsInScreen()` will work.
  SubmenuView* const submenu = item->GetParentMenuItem()->GetSubmenu();
  ShowSubmenu(submenu, [&](auto& params) {
    // Set the bounds such that `item` (which is assumed to be the sole
    // content of `submenu`) has screen bounds `parent_bounds`.
    params.bounds = parent_bounds;
    params.bounds.Inset(-submenu->GetScrollViewContainer()->GetInsets());
  });

  const gfx::Rect final_bounds =
      CalculateBubbleMenuBoundsWithoutInsets(options, item);
  EXPECT_TRUE(monitor_bounds.Contains(final_bounds))
      << monitor_bounds.ToString() << " does not contain "
      << final_bounds.ToString();

  submenu->Close();
}

void MenuControllerTest::SetPendingStateItem(MenuItemView* item) {
  menu_controller_->pending_state_.item = item;
  menu_controller_->pending_state_.submenu_open = true;
}

void MenuControllerTest::SetState(MenuItemView* item) {
  SetPendingStateItem(item);
  menu_controller_->state_.item = item;
  menu_controller_->state_.submenu_open = true;
}

void MenuControllerTest::IncrementSelection() {
  menu_controller_->IncrementSelection(
      MenuController::INCREMENT_SELECTION_DOWN);
}

void MenuControllerTest::DecrementSelection() {
  menu_controller_->IncrementSelection(MenuController::INCREMENT_SELECTION_UP);
}

void MenuControllerTest::DestroyMenuControllerOnMenuClosed(
    TestMenuControllerDelegate* delegate) {
  // Unretained() is safe here as the test should outlive the delegate. If not
  // we want to know.
  delegate->set_on_menu_closed_callback(base::BindRepeating(
      &MenuControllerTest::DestroyMenuController, base::Unretained(this)));
}

void MenuControllerTest::DestroyMenuControllerDelegate() {
  menu_controller_delegate_.reset();
}

MenuItemView* MenuControllerTest::FindInitialSelectableMenuItemDown(
    MenuItemView* parent) {
  return menu_controller_->FindInitialSelectableMenuItem(
      parent, MenuController::INCREMENT_SELECTION_DOWN);
}

MenuItemView* MenuControllerTest::FindInitialSelectableMenuItemUp(
    MenuItemView* parent) {
  return menu_controller_->FindInitialSelectableMenuItem(
      parent, MenuController::INCREMENT_SELECTION_UP);
}

MenuHostRootView* MenuControllerTest::CreateMenuHostRootView(MenuHost* host) {
  return static_cast<MenuHostRootView*>(host->CreateRootView());
}

void MenuControllerTest::MenuHostOnDragWillStart(MenuHost* host) {
  host->OnDragWillStart();
}

void MenuControllerTest::MenuHostOnDragComplete(MenuHost* host) {
  host->OnDragComplete();
}

void MenuControllerTest::SelectByChar(char16_t character) {
  menu_controller_->SelectByChar(character);
}

void MenuControllerTest::SetDropMenuItem(MenuItemView* target,
                                         MenuDelegate::DropPosition position) {
  menu_controller_->SetDropMenuItem(target, position);
}

void MenuControllerTest::SetComboboxType(
    MenuController::ComboboxType combobox_type) {
  menu_controller_->set_combobox_type(combobox_type);
}

void MenuControllerTest::SetSelectionOnPointerDown(
    SubmenuView* source,
    const ui::MouseEvent& event) {
  const ui::MouseEvent converted_event = ConvertEvent(source, event);
  menu_controller_->SetSelectionOnPointerDown(source, &converted_event);
}

bool MenuControllerTest::ProcessMousePressed(SubmenuView* source,
                                             const ui::MouseEvent& event) {
  return menu_controller_->OnMousePressed(source, ConvertEvent(source, event));
}

bool MenuControllerTest::ProcessMouseDragged(SubmenuView* source,
                                             const ui::MouseEvent& event) {
  return menu_controller_->OnMouseDragged(source, ConvertEvent(source, event));
}

void MenuControllerTest::ProcessMouseReleased(SubmenuView* source,
                                              const ui::MouseEvent& event) {
  menu_controller_->OnMouseReleased(source, ConvertEvent(source, event));
}

void MenuControllerTest::ProcessMouseMoved(SubmenuView* source,
                                           const ui::MouseEvent& event) {
  menu_controller_->OnMouseMoved(source, ConvertEvent(source, event));
}

void MenuControllerTest::ProcessGestureEvent(SubmenuView* source,
                                             const ui::GestureEvent& event) {
  ui::GestureEvent converted_event = ConvertEvent(source, event);
  menu_controller_->OnGestureEvent(source, &converted_event);
}

void MenuControllerTest::ProcessTouchEvent(SubmenuView* source,
                                           const ui::TouchEvent& event) {
  ui::TouchEvent converted_event = ConvertEvent(source, event);
  menu_controller_->OnTouchEvent(source, &converted_event);
}

void MenuControllerTest::Accept(MenuItemView* item, int event_flags) {
  menu_controller_->Accept(item, event_flags);
  views::test::WaitForMenuClosureAnimation();
}

void MenuControllerTest::StartDrag() {
  MenuItemView* const dragged_item =
      menu_item()->GetSubmenu()->GetMenuItemAt(0);
  menu_controller_->state_.item = dragged_item;
  menu_controller_->StartDrag(menu_item()->GetSubmenu(),
                              dragged_item->bounds().CenterPoint());
}

void MenuControllerTest::SetUpMenuControllerForCalculateBounds(
    const MenuBoundsOptions& options,
    MenuItemView* menu_item) {
  // Must set both `state_` and `pending_state_` in case, while processing, the
  // controller commits the pending state.
  menu_controller_->pending_state_.anchor = menu_controller_->state_.anchor =
      options.menu_anchor;
  menu_controller_->pending_state_.initial_bounds =
      menu_controller_->state_.initial_bounds = options.anchor_bounds;
  menu_controller_->pending_state_.monitor_bounds =
      menu_controller_->state_.monitor_bounds = options.monitor_bounds;
  menu_item->set_actual_menu_position(options.menu_position);
  menu_item->GetSubmenu()->GetScrollViewContainer()->SetPreferredSize(
      options.menu_size);
}

MenuItemView* MenuControllerTest::AddButtonMenuItems(bool single_child) {
  MenuItemView* const item_view = menu_item()->AppendMenuItem(5, u"Five");
  const size_t children_count = single_child ? 1 : 3;
  for (size_t i = 0; i < children_count; ++i) {
    item_view
        ->AddChildView(
            std::make_unique<LabelButton>(Button::PressedCallback(), u"Label"))
        // This is an in-menu button. Hence it must be always focusable.
        ->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  }
  ShowSubmenu();
  return item_view;
}

void MenuControllerTest::DestroyMenuItem() {
  menu_item_.reset();
}

void MenuControllerTest::SetHotTrackedButton(Button* hot_button) {
  menu_controller_->SetHotTrackedButton(hot_button);
}

void MenuControllerTest::ExitMenuRun() {
  menu_controller_->SetExitType(MenuController::ExitType::kOutermost);
  menu_controller_->ExitTopMostMenu();
}

void MenuControllerTest::DestroyMenuController() {
  if (!menu_controller_) {
    return;
  }

  if (!owner_->IsClosed()) {
    owner_->RemoveObserver(menu_controller_);
  }

  menu_controller_->showing_ = false;
  menu_controller_->owner_ = nullptr;
  delete menu_controller_.ExtractAsDangling();
}

// static
bool MenuControllerTest::SelectionWraps() {
  return MenuConfig::instance().arrow_key_selection_wraps;
}

void MenuControllerTest::OpenMenu(MenuItemView* parent,
                                  const MenuBoundsOptions& options) {
  SetUpMenuControllerForCalculateBounds(options, parent);
  menu_controller_->OpenMenuImpl(parent, true);
}

gfx::Insets MenuControllerTest::GetBorderAndShadowInsets(bool is_submenu) {
  const MenuConfig& menu_config = MenuConfig::instance();
  int elevation = menu_config.bubble_menu_shadow_elevation;
  BubbleBorder::Shadow shadow_type = BubbleBorder::STANDARD_SHADOW;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Increase the submenu shadow elevation and change the shadow style to
  // ChromeOS system UI shadow style when using Ash System UI layout.
  if (menu_controller_->use_ash_system_ui_layout()) {
    if (is_submenu) {
      elevation = menu_config.bubble_submenu_shadow_elevation;
    }
    shadow_type = BubbleBorder::CHROMEOS_SYSTEM_UI_SHADOW;
  }
#endif
  return BubbleBorder::GetBorderAndShadowInsets(elevation, shadow_type);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MenuControllerTest,
                         testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "RTL" : "LTR";
                         });

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

#if BUILDFLAG(IS_OZONE_X11)
// Tests that touch event ids are released correctly. See crbug.com/439051 for
// details. When the ids aren't managed correctly, we get stuck down touches.
TEST_F(MenuControllerTest, TouchIdsReleasedCorrectly) {
  // Run this test only for X11.
  if (ui::OzonePlatform::GetPlatformNameForTest() != "x11") {
    GTEST_SKIP();
  }

  TestEventHandler test_event_handler;
  GetRootWindow(owner())->AddPreTargetHandler(&test_event_handler);

  ui::SetUpTouchDevicesForTest({1});

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
#endif  // BUILDFLAG(IS_OZONE_X11)

// Tests that initial selected menu items are correct when items are enabled or
// disabled.
TEST_F(MenuControllerTest, InitialSelectedItem) {
  // Leave items "Two", "Three", and "Four" enabled.
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  submenu->GetMenuItemAt(0)->SetEnabled(false);
  const auto check_has_command = [](const MenuItemView* item, int command) {
    ASSERT_NE(nullptr, item);
    EXPECT_EQ(command, item->GetCommand());
  };
  // The first selectable item should be item "Two".
  check_has_command(FindInitialSelectableMenuItemDown(menu_item()), 2);
  // The last selectable item should be item "Four".
  check_has_command(FindInitialSelectableMenuItemUp(menu_item()), 4);

  // Leave items "One" and "Two" enabled.
  submenu->GetMenuItemAt(0)->SetEnabled(true);
  submenu->GetMenuItemAt(2)->SetEnabled(false);
  submenu->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  check_has_command(FindInitialSelectableMenuItemDown(menu_item()), 1);
  // The last selectable item should be item "Two".
  check_has_command(FindInitialSelectableMenuItemUp(menu_item()), 2);

  // Leave only a single item "One" enabled.
  submenu->GetMenuItemAt(1)->SetEnabled(false);
  // The first selectable item should be item "One".
  check_has_command(FindInitialSelectableMenuItemDown(menu_item()), 1);
  // The last selectable item should be item "One".
  check_has_command(FindInitialSelectableMenuItemUp(menu_item()), 1);

  // Leave only a single item "Three" enabled.
  submenu->GetMenuItemAt(0)->SetEnabled(false);
  submenu->GetMenuItemAt(2)->SetEnabled(true);
  // The first selectable item should be item "Three".
  check_has_command(FindInitialSelectableMenuItemDown(menu_item()), 3);
  // The last selectable item should be item "Three".
  check_has_command(FindInitialSelectableMenuItemUp(menu_item()), 3);

  // Leave only a single item ("Two") selected.
  submenu->GetMenuItemAt(1)->SetEnabled(true);
  submenu->GetMenuItemAt(2)->SetEnabled(false);
  // The first selectable item should be item "Two".
  check_has_command(FindInitialSelectableMenuItemDown(menu_item()), 2);
  // The last selectable item should be item "Two".
  check_has_command(FindInitialSelectableMenuItemUp(menu_item()), 2);
}

// Verifies that the scroll arrow is shown when the menu content
// does not fit within the available bounds.
// (https://crbug.com/338585369)
TEST_F(MenuControllerTest, VerifyScrollArrowShown) {
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  auto* const scroll_container = submenu->GetScrollViewContainer();

  MenuHost::InitParams params;
  params.parent = owner();
  params.bounds = gfx::Rect(GetPreferredSizeForSubmenu(*submenu));
  // Show the menu at its preferred size without restriction
  submenu->ShowAt(params);
  EXPECT_FALSE(scroll_container->scroll_down_button()->GetVisible());
  // decrease the available space by 1 so the contents no longer fit
  params.bounds.set_height(params.bounds.height() - 1);
  submenu->ShowAt(params);
  EXPECT_TRUE(scroll_container->scroll_down_button()->GetVisible());
}

// Verifies that the context menu bubble should prioritize its cached menu
// position (above or below the anchor) after its size updates
// (https://crbug.com/1126244).
TEST_F(MenuControllerTest, VerifyMenuBubblePositionAfterSizeChanges) {
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
                                    border_and_shadow_insets.height());

  MenuBoundsOptions options = {
      .anchor_bounds = gfx::Rect(anchor_point, gfx::Size()),
      .monitor_bounds = kMonitorBounds,
      .menu_anchor = MenuAnchorPosition::kBubbleRight};

  // Case 1: There is insufficient space for the menu below `anchor_point` and
  // there is no cached menu position. The menu should show above the anchor.
  options.menu_size = kMenuSize;
  EXPECT_GT(options.anchor_bounds.y() - border_and_shadow_insets.height() +
                kMenuSize.height(),
            kMonitorBounds.bottom());
  CalculateBubbleMenuBoundsWithoutInsets(options);
  EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
            menu_item_actual_position());

  // Case 2: There is insufficient space for the menu below `anchor_point`. The
  // cached position is below the anchor. The menu should show above the anchor.
  options.menu_position = MenuItemView::MenuPosition::kBelowBounds;
  CalculateBubbleMenuBoundsWithoutInsets(options);
  EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
            menu_item_actual_position());

  // Case 3: There is enough space for the menu below `anchor_point`. The cached
  // menu position is above the anchor. The menu should show above the anchor.
  constexpr gfx::Size kUpdatedSize(kMenuSize.width(), kMenuSize.height() / 2);
  EXPECT_LE(options.anchor_bounds.y() - border_and_shadow_insets.height() +
                kUpdatedSize.height(),
            kMonitorBounds.bottom());
  options.menu_size = kUpdatedSize;
  options.menu_position = MenuItemView::MenuPosition::kAboveBounds;
  CalculateBubbleMenuBoundsWithoutInsets(options);
  EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
            menu_item_actual_position());
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
                                    border_and_shadow_insets.height());

  MenuBoundsOptions options = {
      .anchor_bounds = gfx::Rect(anchor_point, gfx::Size()),
      .monitor_bounds = kMonitorBounds,
      .menu_anchor = MenuAnchorPosition::kBubbleBottomRight};

  // Case 1: There is insufficient space for the menu below `anchor_point` and
  // there is no cached menu position. The menu should show above the anchor.
  options.menu_size = kMenuSize;
  EXPECT_GT(options.anchor_bounds.y() - border_and_shadow_insets.height() +
                kMenuSize.height(),
            kMonitorBounds.bottom());
  CalculateBubbleMenuBoundsWithoutInsets(options);
  EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
            menu_item_actual_position());

  // Case 2: There is insufficient space for the menu below `anchor_point`. The
  // cached position is below the anchor. The menu should show above the anchor
  // point.
  options.menu_position = MenuItemView::MenuPosition::kBelowBounds;
  CalculateBubbleMenuBoundsWithoutInsets(options);
  EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
            menu_item_actual_position());

  // Case 3: There is enough space for the menu below `anchor_point`. The cached
  // menu position is above the anchor. The menu should show above the anchor.
  constexpr gfx::Size kUpdatedSize(kMenuSize.width(), kMenuSize.height() / 2);
  EXPECT_LE(options.anchor_bounds.y() - border_and_shadow_insets.height() +
                kUpdatedSize.height(),
            kMonitorBounds.bottom());
  options.menu_size = kUpdatedSize;
  options.menu_position = MenuItemView::MenuPosition::kAboveBounds;
  CalculateBubbleMenuBoundsWithoutInsets(options);
  EXPECT_EQ(MenuItemView::MenuPosition::kAboveBounds,
            menu_item_actual_position());

  // Case 4: There is enough space for the menu below `anchor_point`. The cached
  // menu position is below the anchor. The menu should show below the anchor.
  options.menu_position = MenuItemView::MenuPosition::kBelowBounds;
  CalculateBubbleMenuBoundsWithoutInsets(options);
  EXPECT_EQ(MenuItemView::MenuPosition::kBelowBounds,
            menu_item_actual_position());
}

// Tests that opening the menu and pressing 'Home' selects the first menu item.
TEST_F(MenuControllerTest, FirstSelectedItem) {
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  SetPendingStateItem(submenu->GetMenuItemAt(0));
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
  SetPendingStateItem(submenu->GetMenuItemAt(3));
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the first menu item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(1, pending_state_item()->GetCommand());
}

// Tests that opening the menu and pressing 'End' selects the last enabled menu
// item.
TEST_F(MenuControllerTest, LastSelectedItem) {
  // Fake initial root item selection and submenu showing.
  ShowSubmenu();
  SetPendingStateItem(menu_item());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the last item.
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  SetPendingStateItem(submenu->GetMenuItemAt(3));
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the first item.
  SetPendingStateItem(submenu->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());
}

// MenuController tests which set expectations about how menu item selection
// behaves should verify test cases work as intended for all supported selection
// mechanisms.
class MenuControllerSelectionTest : public MenuControllerTest {
 public:
  MenuControllerSelectionTest() = default;

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
      {base::BindRepeating(&MenuControllerSelectionTest::IncrementSelection,
                           base::Unretained(this)),
       base::BindRepeating(&MenuControllerSelectionTest::DecrementSelection,
                           base::Unretained(this))},
      // Updates selection via down/up arrow keys.
      {base::BindRepeating(&MenuControllerTest::DispatchKey,
                           base::Unretained(this),
                           ui::VKEY_DOWN),
       base::BindRepeating(&MenuControllerTest::DispatchKey,
                           base::Unretained(this),
                           ui::VKEY_UP)},
      // Updates selection via next/prior keys.
      {base::BindRepeating(&MenuControllerTest::DispatchKey,
                           base::Unretained(this),
                           ui::VKEY_NEXT),
       base::BindRepeating(&MenuControllerTest::DispatchKey,
                           base::Unretained(this),
                           ui::VKEY_PRIOR)}};
};

// Tests that opening menu and exercising various mechanisms to update
// selection iterates over enabled items.
TEST_F(MenuControllerSelectionTest, NextSelectedItem) {
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  for (const auto& selection_mechanism : selection_mechanisms()) {
    // Disabling the item "Three" gets it skipped when using keyboard to
    // navigate.
    submenu->GetMenuItemAt(2)->SetEnabled(false);

    // Fake initial hot selection.
    SetPendingStateItem(submenu->GetMenuItemAt(0));
    EXPECT_EQ(1, pending_state_item()->GetCommand());

    // Move down in the menu.
    // Select next item.
    selection_mechanism.IncrementSelection.Run();
    EXPECT_EQ(2, pending_state_item()->GetCommand());

    // Skip disabled item.
    selection_mechanism.IncrementSelection.Run();
    EXPECT_EQ(4, pending_state_item()->GetCommand());

    selection_mechanism.IncrementSelection.Run();
    if (SelectionWraps()) {
      // Wrap around.
      EXPECT_EQ(1, pending_state_item()->GetCommand());

      // Move up in the menu.
      // Wrap around.
      selection_mechanism.DecrementSelection.Run();
    }
    EXPECT_EQ(4, pending_state_item()->GetCommand());

    // Skip disabled item.
    selection_mechanism.DecrementSelection.Run();
    EXPECT_EQ(2, pending_state_item()->GetCommand());

    // Select previous item.
    selection_mechanism.DecrementSelection.Run();
    EXPECT_EQ(1, pending_state_item()->GetCommand());
  }
}

// Tests that opening menu and exercising various mechanisms to decrement
// selection selects the last enabled menu item.
TEST_F(MenuControllerSelectionTest, PreviousSelectedItem) {
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  for (const auto& selection_mechanism : selection_mechanisms()) {
    // Disabling the item "Four" gets it skipped when using keyboard to
    // navigate.
    submenu->GetMenuItemAt(3)->SetEnabled(false);

    // Fake initial root item selection and submenu showing.
    SetPendingStateItem(menu_item());
    EXPECT_EQ(0, pending_state_item()->GetCommand());

    // Move up and select a previous (in our case the last enabled) item.
    selection_mechanism.DecrementSelection.Run();
    EXPECT_EQ(3, pending_state_item()->GetCommand());
  }
}

// Tests that the APIs related to the current selected item work correctly.
TEST_F(MenuControllerTest, CurrentSelectedItem) {
  ShowSubmenu();
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the first menu-item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(pending_state_item(), menu_controller()->GetSelectedMenuItem());

  // The API should let the submenu stay open if already so, but clear any
  // selections within it.
  EXPECT_TRUE(showing());
  EXPECT_EQ(1, pending_state_item()->GetCommand());
  menu_controller()->SelectItemAndOpenSubmenu(menu_item());
  EXPECT_TRUE(showing());
  EXPECT_EQ(0, pending_state_item()->GetCommand());
}

// Tests that opening menu and calling SelectByChar works correctly.
TEST_F(MenuControllerTest, SelectByChar) {
  SetComboboxType(MenuController::ComboboxType::kReadonly);
  ShowSubmenu();

  // Handle null character should do nothing.
  SelectByChar(0);
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());
}

TEST_F(MenuControllerTest, SelectChildButtonView) {
  AddButtonMenuItems(/*single_child=*/false);
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const View* const buttons_view = submenu->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  GET_CHILD_BUTTON(button1, buttons_view, 0);
  GET_CHILD_BUTTON(button2, buttons_view, 1);
  GET_CHILD_BUTTON(button3, buttons_view, 2);

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
  const gfx::Point location = View::ConvertPointToTarget(
      button1, submenu, button1->GetLocalBounds().CenterPoint());
  ProcessMouseMoved(
      submenu, ui::MouseEvent(ui::EventType::kMouseMoved, location, location,
                              ui::EventTimeForNow(), 0, 0));
  EXPECT_EQ(button1, hot_button());
  EXPECT_TRUE(button1->IsHotTracked());

  // Incrementing selection should move hot tracking to the second button
  // (next after the first button).
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment selection twice to wrap around.
  IncrementSelection();
  IncrementSelection();
  EXPECT_EQ(SelectionWraps() ? 1 : 5, pending_state_item()->GetCommand());
}

TEST_F(MenuControllerTest, DeleteChildButtonView) {
  AddButtonMenuItems(/*single_child=*/false);

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  const View* const buttons_view = menu_item()->GetSubmenu()->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  GET_CHILD_BUTTON(button1, buttons_view, 0);
  GET_CHILD_BUTTON(button2, buttons_view, 1);
  GET_CHILD_BUTTON(button3, buttons_view, 2);
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
  MenuItemView* const hosting_menu_item =
      AddButtonMenuItems(/*single_child=*/true);
  ASSERT_FALSE(hosting_menu_item->IsSelected());
  GET_CHILD_BUTTON(button, hosting_menu_item, 0);
  EXPECT_FALSE(button->IsHotTracked());

  menu_controller()->SelectItemAndOpenSubmenu(hosting_menu_item);
  EXPECT_TRUE(hosting_menu_item->IsSelected());
  EXPECT_TRUE(button->IsHotTracked());
}

// Verifies that the child button of the menu item which is under mouse
// hovering is hot tracked (https://crbug.com/1135000).
TEST_F(MenuControllerTest, ChildButtonHotTrackedAfterMouseMove) {
  // Add a menu item which owns a button as child.
  const MenuItemView* const hosting_menu_item =
      AddButtonMenuItems(/*single_child=*/true);
  GET_CHILD_BUTTON(button, hosting_menu_item, 0);
  EXPECT_FALSE(button->IsHotTracked());

  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location = View::ConvertPointToTarget(
      button, submenu, button->GetLocalBounds().CenterPoint());
  ProcessMouseMoved(
      submenu, ui::MouseEvent(ui::EventType::kMouseMoved, location, location,
                              ui::EventTimeForNow(), 0, 0));

  // After the mouse moves to `button`, `button` should be hot tracked.
  EXPECT_EQ(button, hot_button());
  EXPECT_TRUE(button->IsHotTracked());
}

// Creates a menu with Button child views, simulates running a nested
// menu and tests that existing the nested run restores hot-tracked child
// view.
TEST_F(MenuControllerTest, ChildButtonHotTrackedWhenNested) {
  AddButtonMenuItems(/*single_child=*/false);

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  const View* const buttons_view = menu_item()->GetSubmenu()->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  GET_CHILD_BUTTON(button1, buttons_view, 0);
  GET_CHILD_BUTTON(button2, buttons_view, 1);
  GET_CHILD_BUTTON(button3, buttons_view, 2);
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
  EXPECT_EQ(button2, hot_button());

  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  // |button2| should stay in hot-tracked state but menu controller should not
  // track it anymore (preventing resetting hot-tracked state when changing
  // selection while a nested run is active).
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_EQ(nullptr, hot_button());

  // Setting hot-tracked button while nested should get reverted when nested
  // menu run ends.
  SetHotTrackedButton(button1);
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_EQ(button1, hot_button());

  // Setting the hot tracked state twice on the same button via the
  // menu controller should still set the hot tracked state on the button
  // again.
  button1->SetHotTracked(false);
  SetHotTrackedButton(button1);
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_EQ(button1, hot_button());

  ExitMenuRun();
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_EQ(button2, hot_button());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when Accept is called.
TEST_F(MenuControllerTest, AsynchronousAccept) {
  views::test::DisableMenuClosureAnimations();

  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());

  MenuItemView* const accepted = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  constexpr int kEventFlags = 42;
  Accept(accepted, kEventFlags);

  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(accepted, menu_controller_delegate()->on_menu_closed_menu());
  EXPECT_EQ(kEventFlags,
            menu_controller_delegate()->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            menu_controller_delegate()->on_menu_closed_notify_type());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when CancelAll is called.
TEST_F(MenuControllerTest, AsynchronousCancelAll) {
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());

  menu_controller()->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(nullptr, menu_controller_delegate()->on_menu_closed_menu());
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            menu_controller_delegate()->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
}

// Tests that canceling a nested menu restores the previous
// MenuControllerDelegate, and notifies each delegate.
TEST_F(MenuControllerTest, AsynchronousNestedDelegate) {
  auto nested_delegate = std::make_unique<TestMenuControllerDelegate>();
  menu_controller()->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), current_controller_delegate());
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  menu_controller()->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(menu_controller_delegate(), current_controller_delegate());
  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
}

// Tests that dropping within an asynchronous menu stops the menu from showing
// and does not notify the controller.
TEST_F(MenuControllerTest, AsynchronousPerformDrop) {
  SubmenuView* const source = menu_item()->GetSubmenu();
  MenuItemView* const target = source->GetMenuItemAt(0);

  SetDropMenuItem(target, MenuDelegate::DropPosition::kAfter);

  ui::OSExchangeData drop_data;
  gfx::PointF location(target->origin());
  const ui::DropTargetEvent target_event(drop_data, location, location,
                                         ui::DragDropTypes::DRAG_MOVE);
  DragOperation output_drag_op = DragOperation::kNone;
  menu_controller()
      ->GetDropCallback(source, target_event)
      .Run(target_event, output_drag_op,
           /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(static_cast<test::TestMenuDelegate*>(target->GetDelegate())
                  ->is_drop_performed());
  EXPECT_FALSE(showing());
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());
}

// Tests that dragging within an asynchronous menu notifies the
// MenuControllerDelegate for shutdown.
TEST_F(MenuControllerTest, AsynchronousDragComplete) {
  TestDragCompleteThenDestroyOnMenuClosed();

  menu_controller()->OnDragWillStart();
  menu_controller()->OnDragComplete(true);

  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(nullptr, menu_controller_delegate()->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            menu_controller_delegate()->on_menu_closed_notify_type());
}

// Tests that if Cancel is called during a drag, that OnMenuClosed is still
// notified when the drag completes.
TEST_F(MenuControllerTest, AsynchronousCancelDuringDrag) {
  TestDragCompleteThenDestroyOnMenuClosed();

  menu_controller()->OnDragWillStart();
  menu_controller()->Cancel(MenuController::ExitType::kAll);
  menu_controller()->OnDragComplete(true);

  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(nullptr, menu_controller_delegate()->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            menu_controller_delegate()->on_menu_closed_notify_type());
}

// Tests that if a menu is destroyed while drag operations are occurring, that
// the MenuHost does not crash as the drag completes.
TEST_F(MenuControllerTest, AsynchronousDragHostDeleted) {
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  MenuHost* const host = menu_host_for_submenu(submenu);
  MenuHostOnDragWillStart(host);
  submenu->Close();
  DestroyMenuItem();
  MenuHostOnDragComplete(host);
}

// Tests that getting the drop callback stops the menu from showing and
// does not notify the controller.
TEST_F(MenuControllerTest, AsyncDropCallback) {
  SubmenuView* const source = menu_item()->GetSubmenu();
  MenuItemView* const target = source->GetMenuItemAt(0);

  SetDropMenuItem(target, MenuDelegate::DropPosition::kAfter);

  ui::OSExchangeData drop_data;
  gfx::PointF location(target->origin());
  const ui::DropTargetEvent target_event(drop_data, location, location,
                                         ui::DragDropTypes::DRAG_MOVE);
  auto drop_cb = menu_controller()->GetDropCallback(source, target_event);

  const auto* const menu_delegate =
      static_cast<test::TestMenuDelegate*>(target->GetDelegate());
  EXPECT_FALSE(menu_delegate->is_drop_performed());
  EXPECT_FALSE(showing());
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());

  DragOperation output_drag_op;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  EXPECT_TRUE(menu_delegate->is_drop_performed());
}

// Widget destruction and cleanup occurs on the MessageLoop after the
// MenuController has been destroyed. A MenuHostRootView should not attempt to
// access a destroyed MenuController. This test should not cause a crash.
TEST_F(MenuControllerTest, HostReceivesInputBeforeDestruction) {
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location =
      submenu->bounds().bottom_right() + gfx::Vector2d(1, 1);

  // Normally created as the full Widget is brought up. Explicitly created
  // here for testing.
  std::unique_ptr<MenuHostRootView> root_view(
      CreateMenuHostRootView(menu_host_for_submenu(submenu)));
  DestroyMenuController();

  // This should not attempt to access the destroyed MenuController and should
  // not crash.
  root_view->OnMouseMoved(ui::MouseEvent(ui::EventType::kMouseMoved, location,
                                         location, ui::EventTimeForNow(),
                                         ui::EF_LEFT_MOUSE_BUTTON, 0));
}

// Tests that an asynchronous menu nested within an asynchronous menu closes
// both menus, and notifies both delegates.
TEST_F(MenuControllerTest, DoubleAsynchronousNested) {
  // Nested run
  auto nested_delegate = std::make_unique<TestMenuControllerDelegate>();
  menu_controller()->AddNestedDelegate(nested_delegate.get());
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  menu_controller()->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that setting send_gesture_events_to_owner flag forwards gesture
// events to owner and the forwarding stops when the current gesture sequence
// ends.
TEST_F(MenuControllerTest, PreserveGestureForOwner) {
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kBottomCenter, false, false);
  ShowSubmenu();

  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location =
      submenu->GetLocalBounds().bottom_left() + gfx::Vector2d(0, 10);
  const ui::GestureEvent event(
      location.x(), location.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));

  // Gesture events should not be forwarded if the flag is not set.
  EXPECT_EQ(owner_gesture_count(), 0);
  EXPECT_FALSE(menu_controller()->send_gesture_events_to_owner());
  ProcessGestureEvent(submenu, event);
  EXPECT_EQ(owner_gesture_count(), 0);

  // The menu's owner should receive gestures triggered outside the menu.
  menu_controller()->set_send_gesture_events_to_owner(true);
  ProcessGestureEvent(submenu, event);
  EXPECT_EQ(owner_gesture_count(), 1);

  const ui::GestureEvent event2(
      location.x(), location.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureEnd));

  ProcessGestureEvent(submenu, event2);
  EXPECT_EQ(owner_gesture_count(), 2);

  // EventType::kGestureEnd resets the |send_gesture_events_to_owner_| flag, so
  // further gesture events should not be sent to the owner.
  ProcessGestureEvent(submenu, event2);
  EXPECT_EQ(owner_gesture_count(), 2);
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

  // Ensure menu is closed before running with the menu with `child_window` as
  // the NativeView for gestures.
  menu_controller()->Cancel(MenuController::ExitType::kAll);

  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kBottomCenter, false, false,
                         child_window.get());
  ShowSubmenu(nullptr, [&](auto& params) {
    params.native_view_for_gestures = child_window.get();
  });

  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location =
      submenu->GetLocalBounds().bottom_left() + gfx::Vector2d(0, 10);
  const ui::GestureEvent event(
      location.x(), location.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));

  // Gesture events should not be forwarded to either the `child_window` or
  // the hosts native window if the flag is not set.
  EXPECT_EQ(0, owner_gesture_count());
  EXPECT_EQ(0, child_delegate.GetGestureCountAndReset());
  EXPECT_FALSE(menu_controller()->send_gesture_events_to_owner());
  ProcessGestureEvent(submenu, event);
  EXPECT_EQ(0, owner_gesture_count());
  EXPECT_EQ(0, child_delegate.GetGestureCountAndReset());

  // The `child_window` should receive gestures triggered outside the menu.
  menu_controller()->set_send_gesture_events_to_owner(true);
  ProcessGestureEvent(submenu, event);
  EXPECT_EQ(0, owner_gesture_count());
  EXPECT_EQ(1, child_delegate.GetGestureCountAndReset());

  const ui::GestureEvent event2(
      location.x(), location.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureEnd));
  ProcessGestureEvent(submenu, event2);
  EXPECT_EQ(0, owner_gesture_count());
  EXPECT_EQ(1, child_delegate.GetGestureCountAndReset());

  // EventType::kGestureEnd resets the `send_gesture_events_to_owner_` flag, so
  // further gesture events should not be sent to the `child_window`.
  ProcessGestureEvent(submenu, event2);
  EXPECT_EQ(0, owner_gesture_count());
  EXPECT_EQ(0, child_delegate.GetGestureCountAndReset());
}
#endif

// Tests that touch outside menu does not closes the menu when forwarding
// gesture events to owner.
TEST_F(MenuControllerTest, NoTouchCloseWhenSendingGesturesToOwner) {
  views::test::DisableMenuClosureAnimations();

  // Owner wants the gesture events.
  menu_controller()->set_send_gesture_events_to_owner(true);

  // Show a sub menu and touch outside of it.
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location =
      submenu->GetLocalBounds().bottom_right() + gfx::Vector2d(1, 1);
  const ui::TouchEvent touch_event(
      ui::EventType::kTouchPressed, location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ProcessTouchEvent(submenu, touch_event);

  // Menu should still be visible.
  EXPECT_TRUE(showing());

  // The current gesture sequence ends.
  ProcessGestureEvent(
      submenu,
      ui::GestureEvent(location.x(), location.y(), 0, ui::EventTimeForNow(),
                       ui::GestureEventDetails(ui::EventType::kGestureEnd)));

  // Touch outside again and menu should be closed.
  ProcessTouchEvent(submenu, touch_event);
  views::test::WaitForMenuClosureAnimation();
  EXPECT_FALSE(showing());
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
}

// Tests that a nested menu does not crash when trying to repost events that
// occur outside of the bounds of the menu. Instead a proper shutdown should
// occur.
TEST_F(MenuControllerTest, AsynchronousRepostEvent) {
  views::test::DisableMenuClosureAnimations();

  auto nested_delegate = std::make_unique<TestMenuControllerDelegate>();
  menu_controller()->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), current_controller_delegate());
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a pointer selection. However have the
  // event occur outside of the bounds of the entire menu.
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location =
      submenu->GetLocalBounds().bottom_right() + gfx::Vector2d(1, 1);

  // When attempting to select outside of all menus this should lead to a
  // shutdown. This should not crash while attempting to repost the event.
  SetSelectionOnPointerDown(
      submenu,
      ui::MouseEvent(ui::EventType::kMousePressed, location, location,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  views::test::WaitForMenuClosureAnimation();

  EXPECT_EQ(menu_controller_delegate(), current_controller_delegate());
  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
}

// Tests that an asynchronous menu reposts touch events that occur outside of
// the bounds of the menu, and that the menu closes.
TEST_F(MenuControllerTest, AsynchronousTouchEventRepostEvent) {
  views::test::DisableMenuClosureAnimations();

  // Show a sub menu to target with a touch event. However have the event
  // occur outside of the bounds of the entire menu.
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location =
      submenu->GetLocalBounds().bottom_right() + gfx::Vector2d(1, 1);
  ProcessTouchEvent(
      submenu,
      ui::TouchEvent(ui::EventType::kTouchPressed, location,
                     ui::EventTimeForNow(),
                     ui::PointerDetails(ui::EventPointerType::kTouch, 0)));
  views::test::WaitForMenuClosureAnimation();

  EXPECT_FALSE(showing());
  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(nullptr, menu_controller_delegate()->on_menu_closed_menu());
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            menu_controller_delegate()->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
}

// Tests that having the MenuController deleted during RepostEvent does not
// cause a crash. ASAN bots should not detect use-after-free in
// MenuController.
TEST_F(MenuControllerTest, AsynchronousRepostEventDeletesController) {
  views::test::DisableMenuClosureAnimations();
  auto nested_delegate = std::make_unique<TestMenuControllerDelegate>();
  menu_controller()->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), current_controller_delegate());
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a pointer selection. However have the
  // event occur outside of the bounds of the entire menu.
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location =
      submenu->GetLocalBounds().bottom_right() + gfx::Vector2d(1, 1);

  // This will lead to MenuController being deleted during the event repost.
  // The remainder of this test, and TearDown should not crash.
  DestroyMenuControllerOnMenuClosed(nested_delegate.get());

  // When attempting to select outside of all menus this should lead to a
  // shutdown. This should not crash while attempting to repost the event.
  SetSelectionOnPointerDown(
      submenu,
      ui::MouseEvent(ui::EventType::kMousePressed, location, location,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  views::test::WaitForMenuClosureAnimation();

  // Close to remove observers before test TearDown
  submenu->Close();
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that having the MenuController deleted during OnGestureEvent does not
// cause a crash. ASAN bots should not detect use-after-free in
// MenuController.
TEST_F(MenuControllerTest, AsynchronousGestureDeletesController) {
  views::test::DisableMenuClosureAnimations();

  auto nested_delegate = std::make_unique<TestMenuControllerDelegate>();
  menu_controller()->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), current_controller_delegate());
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a tap event.
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const gfx::Point location = submenu->GetMenuItemAt(0)->bounds().CenterPoint();

  // This will lead to MenuController being deleted during the processing of
  // the gesture event. The remainder of this test, and TearDown should not
  // crash.
  DestroyMenuControllerOnMenuClosed(nested_delegate.get());
  ProcessGestureEvent(
      submenu,
      ui::GestureEvent(location.x(), location.y(), 0, ui::EventTimeForNow(),
                       ui::GestureEventDetails(ui::EventType::kGestureTap)));
  views::test::WaitForMenuClosureAnimation();

  // Close to remove observers before test TearDown
  submenu->Close();
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Test that the menu is properly placed where it best fits.
TEST_F(MenuControllerTest, CalculateMenuBoundsBestFitTest) {
  const bool ignore_screen_bounds_for_menus =
      ShouldIgnoreScreenBoundsForMenus();

  // Fits in all locations -> placed below.
  MenuBoundsOptions options;
  options.anchor_bounds =
      gfx::Rect(options.menu_size.width(), options.menu_size.height(), 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.anchor_bounds.bottom() + options.menu_size.height());
  gfx::Rect expected(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                     options.menu_size.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits above and to both sides -> placed above.
  options.monitor_bounds.set_height(options.anchor_bounds.bottom());
  expected.set_y(
      options.anchor_bounds.y() -
      (ignore_screen_bounds_for_menus ? 0 : options.menu_size.height()));
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits on both sides, prefer right -> placed right.
  options.anchor_bounds.set_y(options.menu_size.height() / 2);
  options.monitor_bounds.set_height(options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected.set_y(options.anchor_bounds.y());
  } else {
    expected.set_origin(
        {options.anchor_bounds.right(), options.monitor_bounds.y()});
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits only on left -> placed left.
  options.monitor_bounds.set_width(options.anchor_bounds.right());
  if (!ignore_screen_bounds_for_menus) {
    expected.set_x(options.anchor_bounds.x() - options.menu_size.width());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits on both sides, prefer left -> placed left.
  options.menu_anchor = MenuAnchorPosition::kTopRight;
  options.monitor_bounds.set_width(options.anchor_bounds.right() +
                                   options.menu_size.width());
  if (ignore_screen_bounds_for_menus) {
    expected.set_x(options.anchor_bounds.right() - options.menu_size.width());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits only on right -> placed right.
  options.anchor_bounds.set_x(0);
  expected.set_x(
      options.anchor_bounds.right() -
      (ignore_screen_bounds_for_menus ? options.menu_size.width() : 0));
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

// Tests that the menu is properly placed according to its anchor.
TEST_F(MenuControllerTest, CalculateMenuBoundsAnchorTest) {
  MenuBoundsOptions options = {.menu_anchor = MenuAnchorPosition::kTopLeft};
  gfx::Rect expected(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                     options.menu_size.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_anchor = MenuAnchorPosition::kTopRight;
  expected.set_x(options.anchor_bounds.right() - options.menu_size.width());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Menu will be placed above or below with an offset.
  options.menu_anchor = MenuAnchorPosition::kBottomCenter;
  constexpr int kTouchYPadding = 15;

  // Menu fits above -> placed above.
  expected.set_origin(
      {options.anchor_bounds.x() +
           (options.anchor_bounds.width() - options.menu_size.width()) / 2,
       options.anchor_bounds.y() - options.menu_size.height() -
           kTouchYPadding});
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Menu does not fit above -> placed below.
  options.anchor_bounds = gfx::Rect(options.menu_size.width(),
                                    options.menu_size.height() / 2, 0, 0);
  expected.set_origin(
      {options.anchor_bounds.x() +
           (options.anchor_bounds.width() - options.menu_size.width()) / 2,
       (ShouldIgnoreScreenBoundsForMenus()
            ? (-options.anchor_bounds.bottom() - kTouchYPadding)
            : options.anchor_bounds.y() + kTouchYPadding)});
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

// Regression test for https://crbug.com/1217711
TEST_F(MenuControllerTest, MenuAnchorPositionFlippedInRtl) {
  ASSERT_FALSE(base::i18n::IsRTL());

  // Test the AdjustAnchorPositionForRtl() method directly, rather than
  // running the menu, because it's awkward to access the menu's window. Also,
  // the menu bounds are already tested separately.
  constexpr struct {
    MenuAnchorPosition original_position;
    MenuAnchorPosition mirrored_position;
  } kPositions[] = {
      {MenuAnchorPosition::kTopLeft, MenuAnchorPosition::kTopRight},
      {MenuAnchorPosition::kBubbleTopLeft, MenuAnchorPosition::kBubbleTopRight},
      {MenuAnchorPosition::kBubbleLeft, MenuAnchorPosition::kBubbleRight},
      {MenuAnchorPosition::kBubbleBottomLeft,
       MenuAnchorPosition::kBubbleBottomRight}};

  for (const auto& position : kPositions) {
    EXPECT_EQ(position.original_position,
              AdjustAnchorPositionForRtl(position.original_position));
    EXPECT_EQ(position.mirrored_position,
              AdjustAnchorPositionForRtl(position.mirrored_position));
  }

  base::i18n::SetRTLForTesting(true);

  // Anchor positions are left/right flipped in RTL.
  for (const auto& position : kPositions) {
    EXPECT_EQ(position.mirrored_position,
              AdjustAnchorPositionForRtl(position.original_position));
    EXPECT_EQ(position.original_position,
              AdjustAnchorPositionForRtl(position.mirrored_position));
  }

  base::i18n::SetRTLForTesting(false);
}

TEST_F(MenuControllerTest, CalculateMenuBoundsMonitorFitTest) {
  constexpr gfx::Rect kMonitorBounds(0, 0, 100, 100);
  MenuBoundsOptions options = {
      .anchor_bounds = gfx::Rect(),
      .monitor_bounds = kMonitorBounds,
      .menu_size =
          gfx::Size(kMonitorBounds.width() / 2, kMonitorBounds.height() * 2)};
  gfx::Rect expected(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                     options.menu_size.width(), kMonitorBounds.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_size =
      gfx::Size(kMonitorBounds.width() * 2, kMonitorBounds.height() / 2);
  expected.set_size({kMonitorBounds.width(), options.menu_size.height()});
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_size =
      gfx::Size(kMonitorBounds.width() * 2, kMonitorBounds.height() * 2);
  expected.set_size(kMonitorBounds.size());
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

// Test that menus show up on screen with non-zero sized anchors.
TEST_P(MenuControllerTest, TestMenuFitsOnScreen) {
  // Simulate multiple display layouts.
  constexpr int kDisplaySize = 500;
  constexpr int kCoords[] = {-kDisplaySize, 0, kDisplaySize};
  for (int x : kCoords) {
    for (int y : kCoords) {
      const gfx::Rect monitor_bounds(x, y, kDisplaySize, kDisplaySize);
      for (auto position : kBubblePositions) {
        TestMenuFitsOnScreen(position, monitor_bounds);
      }
    }
  }
}

// Test that menus show up on screen with zero sized anchors.
TEST_P(MenuControllerTest, TestMenuFitsOnScreenSmallAnchor) {
  // Simulate multiple display layouts.
  constexpr int kDisplaySize = 500;
  constexpr int kCoords[] = {-kDisplaySize, 0, kDisplaySize};
  for (int x : kCoords) {
    for (int y : kCoords) {
      const gfx::Rect monitor_bounds(x, y, kDisplaySize, kDisplaySize);
      for (auto position : kBubblePositions) {
        TestMenuFitsOnScreenSmallAnchor(position, monitor_bounds);
      }
    }
  }
}

// Test that menus fit a small screen.
TEST_P(MenuControllerTest, TestMenuFitsOnSmallScreen) {
  // Simulate multiple display layouts.
  constexpr int kDisplaySize = 500;
  constexpr int kCoords[] = {-kDisplaySize, 0, kDisplaySize};
  for (int x : kCoords) {
    for (int y : kCoords) {
      const gfx::Rect monitor_bounds(x, y, kDisplaySize, kDisplaySize);
      for (auto position : kBubblePositions) {
        TestMenuFitsOnSmallScreen(position, monitor_bounds);
      }
    }
  }
}

// Test that submenus are displayed within the screen bounds on smaller
// screens.
TEST_P(MenuControllerTest, TestSubmenuFitsOnScreen) {
  menu_controller()->set_use_ash_system_ui_layout(true);
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const std::vector<MenuItemView*> menu_items = submenu->GetMenuItems();
  base::ranges::for_each(
      base::make_span(menu_items).subspan(1),
      [&](auto* item) { menu_item()->RemoveMenuItem(item); });
  MenuItemView* const sub_item = submenu->GetMenuItemAt(0);
  sub_item->AppendMenuItem(11, u"Subitem.One");

  const int menu_width = MenuConfig::instance().touchable_menu_min_width;
  const gfx::Size parent_size(menu_width, menu_width);
  const gfx::Size parent_size_wide(menu_width * 2, menu_width);
  const int display_width = parent_size.width() * 3;
  const int display_height = parent_size.height() * 3;
  for (auto menu_position : {MenuAnchorPosition::kBubbleTopLeft,
                             MenuAnchorPosition::kBubbleTopRight}) {
    // Simulate multiple display layouts.
    for (int x : {-display_width, 0, display_width}) {
      for (int y : {-display_height, 0, display_height}) {
        const gfx::Rect monitor_bounds(x, y, display_width, display_height);
        const int x_min = monitor_bounds.x();
        const int x_max = monitor_bounds.right() - parent_size.width();
        const int y_min = monitor_bounds.y();
        const int y_max = monitor_bounds.bottom() - parent_size.height();
        for (const auto& origin :
             {gfx::Point(x_min, y_min), gfx::Point(x_max, y_min),
              gfx::Point((x_min + x_max) / 2, y_min),
              gfx::Point(x_min, (y_min + y_max) / 2),
              gfx::Point(x_min, y_max)}) {
          TestSubmenuFitsOnScreen(sub_item, monitor_bounds,
                                  gfx::Rect(origin, parent_size),
                                  menu_position);
        }

        // Extra wide menu: test with insufficient room on both sides.
        TestSubmenuFitsOnScreen(
            sub_item, monitor_bounds,
            gfx::Rect(gfx::Point(x_min + (x_max - x_min) / 4, y_min),
                      parent_size_wide),
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
  if (ShouldIgnoreScreenBoundsForMenus()) {
    return;
  }

  MenuBoundsOptions options = {
      // The anchor should be near the bottom right side of the screen.
      .anchor_bounds = gfx::Rect(80, 70, 15, 10),
      .monitor_bounds = gfx::Rect(0, 0, 100, 100),
      // The menu should fit the available space, below the anchor.
      .menu_size = gfx::Size(20, 20),
  };

  // Ensure the menu is initially drawn below the bounds, and the MenuPosition
  // is set to MenuPosition::kBelowBounds;
  EXPECT_EQ(gfx::Rect(80, 80, 20, 20), CalculateMenuBounds(options));
  EXPECT_EQ(MenuItemView::MenuPosition::kBelowBounds,
            menu_item_actual_position());

  // The menu bounds are larger than the remaining space on the monitor. This
  // simulates the case where the menu has been grown vertically and
  // horizontally to where it would no longer fit on the screen.
  options.menu_size = gfx::Size(50, 50);
  options.menu_position = MenuItemView::MenuPosition::kBelowBounds;

  // The menu bounds should move left to show the wider menu, and grow to fill
  // the remaining vertical space without moving upwards.
  EXPECT_EQ(gfx::Rect(50, 80, 50, 20), CalculateMenuBounds(options));
}

#if defined(USE_AURA)
// This tests that mouse moved events from the initial position of the mouse
// when the menu was shown don't select the menu item at the mouse position.
TEST_F(MenuControllerTest, MouseAtMenuItemOnShow) {
  // Most tests create an already shown menu but this test needs one that's
  // not shown, so it can show it. The mouse position is remembered when
  // the menu is shown.
  auto menu_item = std::make_unique<MenuItemView>(menu_delegate());
  const MenuItemView* const first_item = menu_item->AppendMenuItem(1, u"One");
  menu_item->AppendMenuItem(2, u"Two");
  menu_item->set_controller(menu_controller());

  // Move the mouse to where the first menu item will be shown,
  // and show the menu.
  const gfx::Size item_size = first_item->CalculatePreferredSize({});
  gfx::Point location(item_size.width() / 2, item_size.height() / 2);
  GetRootWindow(owner())->MoveCursorTo(location);
  menu_controller()->Run(owner(), nullptr, menu_item.get(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Synthesize an event at the mouse position when the menu was opened.
  // It should be ignored, and selected item shouldn't change.
  SubmenuView* const submenu = menu_item->GetSubmenu();
  View::ConvertPointFromScreen(submenu, &location);
  ProcessMouseMoved(
      submenu, ui::MouseEvent(ui::EventType::kMouseMoved, location, location,
                              ui::EventTimeForNow(), 0, 0));
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Synthesize an event at a slightly different mouse position. It
  // should cause the item under the cursor to be selected.
  location.Offset(0, 1);
  ProcessMouseMoved(
      submenu, ui::MouseEvent(ui::EventType::kMouseMoved, location, location,
                              ui::EventTimeForNow(), 0, 0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());
}

// Tests that when an asynchronous menu receives a cancel event, that it
// closes.
TEST_F(MenuControllerTest, AsynchronousCancelEvent) {
  ExitMenuRun();
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(MenuController::ExitType::kNone, menu_controller()->exit_type());
  ui::CancelModeEvent cancel_event;
  event_generator()->Dispatch(&cancel_event);
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
}

TEST_F(MenuControllerTest, WidgetStateChangeCancelsMenu) {
  ExitMenuRun();
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_TRUE(showing());
  EXPECT_EQ(MenuController::ExitType::kNone, menu_controller()->exit_type());
  owner()->SetFullscreen(true);
  EXPECT_FALSE(showing());
  EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
}

// TODO(pkasting): The test below fails most of the time on Wayland; not clear
// it's important to support this case.
#if BUILDFLAG(ENABLE_DESKTOP_AURA) && !BUILDFLAG(IS_OZONE_WAYLAND)
class DesktopMenuControllerTest : public MenuControllerTest {
 public:
  // MenuControllerTest:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    MenuControllerTest::SetUp();
  }
};

// Tests that menus without parent widgets do not crash in
// MenuPreTargetHandler. Having neither parent nor context pointers when
// creating a Widget is only valid in desktop Aura.
TEST_F(DesktopMenuControllerTest, RunWithoutWidgetDoesntCrash) {
  ExitMenuRun();
  menu_controller()->Run(nullptr, nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
}
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA) && !BUILDFLAG(IS_OZONE_WAYLAND)

// Tests that if a MenuController is destroying during drag/drop, and another
// MenuController becomes active, that the exiting of drag does not cause a
// crash.
TEST_F(MenuControllerTest, MenuControllerReplacedDuringDrag) {
  // Build the menu so that the appropriate root window is available to set
  // the drag drop client on.
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
  // Build the menu so that the appropriate root window is available to set
  // the drag drop client on.
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
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  TestDestroyedDuringViewsRelease();
}

// Tests that when a context menu is opened above an empty menu item, and a
// right-click occurs over the empty item, that the bottom menu is not hidden,
// that a request to relaunch the context menu is received, and that
// subsequently pressing ESC does not crash the browser.
TEST_F(MenuControllerTest, RepostEventToEmptyMenuItem) {
  // Setup a submenu. Additionally hook up appropriate Widget and View
  // containers, with bounds, so that hit testing works.
  ShowSubmenu();
  SubmenuView* const base_submenu = menu_item()->GetSubmenu();
  menu_host_for_submenu(base_submenu)
      ->SetContentsView(base_submenu->GetScrollViewContainer());

  // Build the submenu to have an empty menu item. Additionally hook up
  // appropriate Widget and View containers with bounds, so that hit testing
  // works.
  MenuItemView* submenu_item = menu_item()->AppendSubMenu(0, std::u16string());
  submenu_item->UpdateEmptyMenusAndMetrics();
  SubmenuView* const submenu_view = submenu_item->GetSubmenu();
  const auto insets = submenu_view->GetScrollViewContainer()->GetInsets();
  const gfx::Rect bounds(0, 50, 50 + insets.width(), 50 + insets.height());
  // TODO(pkasting): The bounds manipulation in this whole test is suspicious;
  // understand it more deeply, see why the lambda here is needed and if it
  // can be removed or any of this test simplified/clarified.
  ShowSubmenu(submenu_view, [&](auto& params) { params.bounds = bounds; });
  menu_host_for_submenu(submenu_view)
      ->SetContentsView(submenu_view->GetScrollViewContainer());

  // Set that the last selection target was the item which launches the
  // submenu, as the empty item can never become a target.
  SetPendingStateItem(submenu_item);

  // Nest a context menu.
  auto nested_menu_delegate_1 = std::make_unique<test::TestMenuDelegate>();
  auto nested_menu_item_1 =
      std::make_unique<MenuItemView>(nested_menu_delegate_1.get());
  nested_menu_item_1->set_controller(menu_controller());
  SubmenuView* const nested_menu_submenu = nested_menu_item_1->CreateSubmenu();
  ShowSubmenu(nested_menu_submenu);
  menu_host_for_submenu(nested_menu_submenu)
      ->SetContentsView(nested_menu_submenu->GetScrollViewContainer());
  auto nested_controller_delegate_1 =
      std::make_unique<TestMenuControllerDelegate>();
  menu_controller()->AddNestedDelegate(nested_controller_delegate_1.get());
  menu_controller()->Run(owner(), nullptr, nested_menu_item_1.get(),
                         gfx::Rect(150, 50, 100, 100),
                         MenuAnchorPosition::kTopLeft, true, false);

  // Press down outside of the context menu, and within the empty menu item.
  // This should close the first context menu.
  gfx::Point press_location = submenu_view->GetLocalBounds().CenterPoint();
  const gfx::Point press_location_for_nested_menu =
      View::ConvertPointFromScreen(
          nested_menu_submenu,
          View::ConvertPointToScreen(submenu_view, press_location));
  ProcessMousePressed(
      nested_menu_submenu,
      ui::MouseEvent(ui::EventType::kMousePressed,
                     press_location_for_nested_menu,
                     press_location_for_nested_menu, ui::EventTimeForNow(),
                     ui::EF_RIGHT_MOUSE_BUTTON, 0));
  EXPECT_EQ(nested_controller_delegate_1->on_menu_closed_called(), 1);
  EXPECT_EQ(menu_controller_delegate(), current_controller_delegate());

  // While the current state is the menu item which launched the sub menu,
  // cause a drag in the empty menu item. This should not hide the menu.
  SetState(submenu_item);
  press_location.Offset(-5, 0);
  ProcessMouseDragged(
      submenu_view, ui::MouseEvent(ui::EventType::kMouseDragged, press_location,
                                   press_location, ui::EventTimeForNow(),
                                   ui::EF_RIGHT_MOUSE_BUTTON, 0));
  ASSERT_EQ(menu_delegate()->will_hide_menu_count(), 0);

  // Release the mouse in the empty menu item, triggering a context menu
  // request.
  ProcessMouseReleased(
      submenu_view,
      ui::MouseEvent(ui::EventType::kMouseReleased, press_location,
                     press_location, ui::EventTimeForNow(),
                     ui::EF_RIGHT_MOUSE_BUTTON, 0));
  EXPECT_EQ(menu_delegate()->show_context_menu_count(), 1);
  EXPECT_EQ(menu_delegate()->show_context_menu_source(), submenu_item);

  // Nest a context menu.
  auto nested_menu_delegate_2 = std::make_unique<test::TestMenuDelegate>();
  auto nested_menu_item_2 =
      std::make_unique<MenuItemView>(nested_menu_delegate_2.get());
  nested_menu_item_2->set_controller(menu_controller());
  auto nested_controller_delegate_2 =
      std::make_unique<TestMenuControllerDelegate>();
  menu_controller()->AddNestedDelegate(nested_controller_delegate_2.get());
  menu_controller()->Run(owner(), nullptr, nested_menu_item_2.get(),
                         gfx::Rect(150, 50, 100, 100),
                         MenuAnchorPosition::kTopLeft, true, false);

  // The escape key should only close the nested menu. SelectByChar should not
  // crash.
  TestAsyncEscapeKey();
  EXPECT_EQ(nested_controller_delegate_2->on_menu_closed_called(), 1);
  EXPECT_EQ(menu_controller_delegate(), current_controller_delegate());
}

// Drag the mouse from an external view into a menu
// When the mouse leaves the menu while still in the process of dragging
// the menu item view highlight should turn off
TEST_F(MenuControllerTest, DragFromViewIntoMenuAndExit) {
  auto drag_view = std::make_unique<View>();
  drag_view->SetBounds(0, 500, 100, 100);
  const gfx::Point press_location = drag_view->GetLocalBounds().CenterPoint();
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const MenuItemView* const first_item = submenu->GetMenuItemAt(0);
  const gfx::Point drag_location = first_item->bounds().CenterPoint();

  // Begin drag on an external view
  drag_view->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, press_location, press_location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));

  // Drag into a menu item
  ProcessMouseDragged(
      submenu,
      ui::MouseEvent(ui::EventType::kMouseDragged, drag_location, drag_location,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  EXPECT_TRUE(first_item->IsSelected());

  // Drag out of the menu item
  constexpr gfx::Point kReleaseLocation(200, 50);
  ProcessMouseDragged(
      submenu, ui::MouseEvent(ui::EventType::kMouseDragged, kReleaseLocation,
                              kReleaseLocation, ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON, 0));
  EXPECT_FALSE(first_item->IsSelected());

  // Complete drag with release
  ProcessMouseReleased(
      submenu, ui::MouseEvent(ui::EventType::kMouseReleased, kReleaseLocation,
                              kReleaseLocation, ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON, 0));
}

// Tests that |MenuHost::InitParams| are correctly forwarded to the created
// |aura::Window|.
TEST_F(MenuControllerTest, AuraWindowIsInitializedWithMenuHostInitParams) {
  constexpr gfx::Rect kAnchorRect(1, 5, 2, 5);
  ShowSubmenu(nullptr, [anchor_rect = kAnchorRect](auto& params) {
    params.owned_window_anchor.anchor_rect = anchor_rect;
  });
  auto* property =
      menu_item()->GetSubmenu()->GetWidget()->GetNativeWindow()->GetProperty(
          aura::client::kOwnedWindowAnchor);
  ASSERT_TRUE(property);
  EXPECT_EQ(kAnchorRect, property->anchor_rect);
}

// Tests that |aura::Window| has the correct properties when a context menu is
// shown.
TEST_F(MenuControllerTest, ContextMenuInitializesAuraWindowWhenShown) {
  // Checking that context menu properties are calculated correctly.
  MenuBoundsOptions options = {.menu_anchor = MenuAnchorPosition::kTopLeft};
  SetUpMenuControllerForCalculateBounds(options, menu_item());
  menu_controller()->Run(owner(), nullptr, menu_item(), options.anchor_bounds,
                         options.menu_anchor, true, false);

  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const aura::Window* window = submenu->GetWidget()->GetNativeWindow();
  const ui::OwnedWindowAnchor* anchor =
      window->GetProperty(aura::client::kOwnedWindowAnchor);
  EXPECT_TRUE(anchor);
  EXPECT_EQ(ui::OwnedWindowAnchorPosition::kBottomLeft,
            anchor->anchor_position);
  EXPECT_EQ(ui::OwnedWindowAnchorGravity::kBottomRight, anchor->anchor_gravity);
  EXPECT_EQ((ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipY |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentRezizeY),
            anchor->constraint_adjustment);
  EXPECT_EQ(CalculateExpectedMenuAnchorRect(menu_item()), anchor->anchor_rect);

  // Checking that child menu properties are calculated correctly.
  MenuItemView* const child_menu = submenu->GetMenuItemAt(0);
  child_menu->CreateSubmenu();
  ASSERT_NE(nullptr, child_menu->GetParentMenuItem());
  options.menu_anchor = MenuAnchorPosition::kTopRight;
  SetUpMenuControllerForCalculateBounds(options, child_menu);
  menu_controller()->Run(owner(), nullptr, child_menu,
                         child_menu->GetBoundsInScreen(), options.menu_anchor,
                         false, false);

  ASSERT_NE(nullptr, child_menu->GetWidget());
  window = child_menu->GetSubmenu()->GetWidget()->GetNativeWindow();

  anchor = window->GetProperty(aura::client::kOwnedWindowAnchor);
  EXPECT_TRUE(anchor);
  EXPECT_EQ(ui::OwnedWindowAnchorPosition::kTopRight, anchor->anchor_position);
  EXPECT_EQ(ui::OwnedWindowAnchorGravity::kBottomRight, anchor->anchor_gravity);
  EXPECT_EQ((ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideY |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentResizeX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentRezizeY),
            anchor->constraint_adjustment);
  EXPECT_EQ(CalculateExpectedMenuAnchorRect(child_menu), anchor->anchor_rect);
}

// Tests that |aura::Window| has the correct properties when a root or a child
// menu is shown.
TEST_F(MenuControllerTest, RootAndChildMenusInitializeAuraWindowWhenShown) {
  // Checking that root menu properties are calculated correctly.
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  MenuBoundsOptions options = {
      .menu_size = GetPreferredSizeForSubmenu(*submenu),
      .menu_anchor = MenuAnchorPosition::kTopLeft};
  SetUpMenuControllerForCalculateBounds(options, menu_item());
  menu_controller()->Run(owner(), nullptr, menu_item(), options.anchor_bounds,
                         options.menu_anchor, false, false);

  const aura::Window* window = submenu->GetWidget()->GetNativeWindow();
  const ui::OwnedWindowAnchor* anchor =
      window->GetProperty(aura::client::kOwnedWindowAnchor);
  EXPECT_TRUE(anchor);
  EXPECT_EQ(ui::OwnedWindowAnchorPosition::kBottomLeft,
            anchor->anchor_position);
  EXPECT_EQ(ui::OwnedWindowAnchorGravity::kBottomRight, anchor->anchor_gravity);
  EXPECT_EQ((ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipY |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentRezizeY),
            anchor->constraint_adjustment);
  EXPECT_EQ(CalculateExpectedMenuAnchorRect(menu_item()), anchor->anchor_rect);

  // Checking that child menu properties are calculated correctly.
  MenuItemView* const child_item = submenu->GetMenuItemAt(0);
  child_item->AppendMenuItem(1, u"Child one");
  SubmenuView* const child_submenu = child_item->GetSubmenu();
  ASSERT_NE(nullptr, child_item->GetParentMenuItem());
  options.menu_size = GetPreferredSizeForSubmenu(*child_submenu);
  options.menu_anchor = MenuAnchorPosition::kTopRight;
  SetUpMenuControllerForCalculateBounds(options, child_item);
  menu_controller()->Run(owner(), nullptr, child_item,
                         child_item->GetBoundsInScreen(), options.menu_anchor,
                         false, false);

  ASSERT_NE(nullptr, child_item->GetWidget());
  window = child_submenu->GetWidget()->GetNativeWindow();

  anchor = window->GetProperty(aura::client::kOwnedWindowAnchor);
  EXPECT_TRUE(anchor);
  EXPECT_EQ(ui::OwnedWindowAnchorPosition::kTopRight, anchor->anchor_position);
  EXPECT_EQ(ui::OwnedWindowAnchorGravity::kBottomRight, anchor->anchor_gravity);
  EXPECT_EQ((ui::OwnedWindowConstraintAdjustment::kAdjustmentSlideY |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentResizeX |
             ui::OwnedWindowConstraintAdjustment::kAdjustmentRezizeY),
            anchor->constraint_adjustment);
  const auto anchor_rect = anchor->anchor_rect;
  EXPECT_EQ(CalculateExpectedMenuAnchorRect(child_item), anchor->anchor_rect);

  // Try to reposition the existing menu. Its anchor must change.
  child_item->SetY(child_item->y() + 2);
  menu_controller()->Run(owner(), nullptr, child_item,
                         child_item->GetBoundsInScreen(),
                         MenuAnchorPosition::kTopLeft, false, false);
  MenuChildrenChanged(child_item);

  EXPECT_EQ(CalculateExpectedMenuAnchorRect(child_item), anchor->anchor_rect);
  // New anchor mustn't be the same as the old one.
  EXPECT_NE(anchor->anchor_rect, anchor_rect);
}

// Test that if `SetTriggerActionWithNonIconChildViews` true that even with
// child views the click will be registered. Detect that the click was
// registered by checking if the TestMenuControllerDelegate received the signal
// that the menu should be closed.
TEST_F(MenuControllerTest, RegisterClickWithChildViews) {
  DestroyMenuControllerOnMenuClosed(menu_controller_delegate());
  ShowSubmenu();
  SubmenuView* submenu = menu_item()->GetSubmenu();
  MenuItemView* first_menu_item = submenu->GetMenuItemAt(0);
  first_menu_item->AddChildView(std::make_unique<View>());
  const gfx::Point press_location = first_menu_item->bounds().CenterPoint();
  ProcessMousePressed(
      submenu, ui::MouseEvent(ui::EventType::kMousePressed, press_location,
                              press_location, ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON, 0));
  ProcessMouseReleased(
      submenu, ui::MouseEvent(ui::EventType::kMouseReleased, press_location,
                              press_location, ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON, 0));
  // No signal when there's a child view and
  // SetTriggerActionWithNonIconChildViews is false.
  EXPECT_EQ(menu_controller_delegate()->on_menu_closed_called(), 0);
  first_menu_item->SetTriggerActionWithNonIconChildViews(true);
  ProcessMousePressed(
      submenu, ui::MouseEvent(ui::EventType::kMousePressed, press_location,
                              press_location, ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON, 0));
  ProcessMouseReleased(
      submenu, ui::MouseEvent(ui::EventType::kMouseReleased, press_location,
                              press_location, ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON, 0));
  // We should receive a signal to close the menu when
  // SetTriggerActionWithNonIconChildViews is true.
  EXPECT_EQ(menu_controller_delegate()->on_menu_closed_called(), 1);
  // Because the menu has been closed, destroy the menu controller delegate as
  // well to avoid dangling pointers to the menu.
  DestroyMenuControllerDelegate();
}

#endif  // defined(USE_AURA)

// Tests that having the MenuController deleted during OnMousePressed does not
// cause a crash. ASAN bots should not detect use-after-free in
// MenuController.
TEST_F(MenuControllerTest, NoUseAfterFreeWhenMenuCanceledOnMousePress) {
  DestroyMenuControllerOnMenuClosed(menu_controller_delegate());

  // Creating own MenuItem for a minimal test environment.
  auto item = std::make_unique<MenuItemView>(menu_delegate());
  item->set_controller(menu_controller());
  item->SetBounds(0, 0, 50, 50);

  SubmenuView* const submenu = item->CreateSubmenu();
  auto* const canceling_view =
      submenu->AddChildView(std::make_unique<CancelMenuOnMousePressView>(
          menu_controller()->AsWeakPtr()));
  canceling_view->SetBoundsRect(item->GetLocalBounds());

  menu_controller()->Run(owner(), nullptr, item.get(), item->bounds(),
                         MenuAnchorPosition::kTopLeft, false, false);
  ShowSubmenu(submenu);

  // Simulate a mouse press in the middle of the |closing_widget|.
  const gfx::Point location = canceling_view->bounds().CenterPoint();
  EXPECT_TRUE(ProcessMousePressed(
      submenu,
      ui::MouseEvent(ui::EventType::kMousePressed, location, location,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0)));

  // Close to remove observers before test TearDown.
  submenu->Close();
}

TEST_F(MenuControllerTest, SetSelectionIndices_MenuItemsOnly) {
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  MenuItemView* const item1 = submenu->GetMenuItemAt(0);
  MenuItemView* const item2 = submenu->GetMenuItemAt(1);
  MenuItemView* const item3 = submenu->GetMenuItemAt(2);
  MenuItemView* const item4 = submenu->GetMenuItemAt(3);
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
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  MenuItemView* const item1 = submenu->GetMenuItemAt(0);
  item1->SetEnabled(false);
  const MenuItemView* const item2 = submenu->GetMenuItemAt(1);
  MenuItemView* const item3 = submenu->GetMenuItemAt(2);
  item3->SetVisible(false);
  const MenuItemView* const item4 = submenu->GetMenuItemAt(3);
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
  AddButtonMenuItems(/*single_child=*/false);
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const MenuItemView* const item1 = submenu->GetMenuItemAt(0);
  const MenuItemView* const item2 = submenu->GetMenuItemAt(1);
  const MenuItemView* const item3 = submenu->GetMenuItemAt(2);
  const MenuItemView* const item4 = submenu->GetMenuItemAt(3);
  const MenuItemView* const item5 = submenu->GetMenuItemAt(4);
  GET_CHILD_BUTTON(button1, item5, 0);
  GET_CHILD_BUTTON(button2, item5, 1);
  GET_CHILD_BUTTON(button3, item5, 2);
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
  AddButtonMenuItems(/*single_child=*/false);
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const MenuItemView* const item1 = submenu->GetMenuItemAt(0);
  const MenuItemView* const item2 = submenu->GetMenuItemAt(1);
  const MenuItemView* const item3 = submenu->GetMenuItemAt(2);
  const MenuItemView* const item4 = submenu->GetMenuItemAt(3);
  const MenuItemView* const item5 = submenu->GetMenuItemAt(4);
  GET_CHILD_BUTTON(button1, item5, 0);
  GET_CHILD_BUTTON(button2, item5, 1);
  GET_CHILD_BUTTON(button3, item5, 2);
  button1->SetEnabled(false);
  button2->SetVisible(false);
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
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const MenuItemView* const item1 = submenu->GetMenuItemAt(0);
  const MenuItemView* const item2 = submenu->GetMenuItemAt(1);
  const MenuItemView* const item3 = submenu->GetMenuItemAt(2);
  MenuItemView* const item4 = submenu->GetMenuItemAt(3);

  // This simulates how buttons are nested in views in the main app menu.
  auto* const container_view = item4->AddChildView(std::make_unique<View>());
  container_view->GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);

  // There's usually a label before the traversable elements.
  container_view->AddChildView(std::make_unique<Label>());

  // Add two focusable buttons (buttons in menus are always focusable).
  auto* const button1 =
      container_view->AddChildView(std::make_unique<LabelButton>());
  button1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button1->GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  auto* const button2 =
      container_view->AddChildView(std::make_unique<LabelButton>());
  button2->GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
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

TEST_F(MenuControllerTest, AccessibleProperties) {
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  MenuScrollViewContainer* scroll_view_container =
      submenu->GetScrollViewContainer();

  ui::AXNodeData data;
  scroll_view_container->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kMenuBar);
}

TEST_F(MenuControllerTest, SetSelectionIndices_ChildrenChanged) {
  AddButtonMenuItems(/*single_child=*/false);
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  MenuItemView* const item1 = submenu->GetMenuItemAt(0);
  MenuItemView* const item2 = submenu->GetMenuItemAt(1);
  const MenuItemView* const item3 = submenu->GetMenuItemAt(2);
  const MenuItemView* const item4 = submenu->GetMenuItemAt(3);
  const MenuItemView* const item5 = submenu->GetMenuItemAt(4);
  GET_CHILD_BUTTON(button1, item5, 0);
  GET_CHILD_BUTTON(button2, item5, 1);
  GET_CHILD_BUTTON(button3, item5, 2);
  OpenMenu(menu_item());

  const auto expect_coordinates = [](const View* v, std::optional<int> pos,
                                     std::optional<int> size) {
    ui::AXNodeData data;
    v->GetViewAccessibility().GetAccessibleNodeData(&data);
    const auto check_attribute = [&](const auto& expected, auto attribute) {
      EXPECT_EQ(expected.has_value(), data.HasIntAttribute(attribute));
      if (expected.has_value()) {
        EXPECT_EQ(expected.value(), data.GetIntAttribute(attribute));
      }
    };
    check_attribute(pos, ax::mojom::IntAttribute::kPosInSet);
    check_attribute(size, ax::mojom::IntAttribute::kSetSize);
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
  const MenuItemView* const item6 = menu_item()->AppendMenuItem(6, u"Six");
  menu_item()->RemoveMenuItem(item2);
  MenuChildrenChanged(menu_item());

  // Verify that disabled menu items no longer have PosInSet or SetSize.
  expect_coordinates(item1, std::nullopt, std::nullopt);
  expect_coordinates(button1, std::nullopt, std::nullopt);
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

  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());

  MenuItemView* const accepted = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  accepted->HandleAccessibleAction(data);
  views::test::WaitForMenuClosureAnimation();

  EXPECT_EQ(1, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_EQ(accepted, menu_controller_delegate()->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            menu_controller_delegate()->on_menu_closed_notify_type());
}

// Test that the kSelectedChildrenChanged event is emitted on
// the root menu item when the selected menu item changes.
TEST_F(MenuControllerTest, AccessibilityEmitsSelectChildrenChanged) {
  const test::AXEventCounter ax_counter(views::AXEventManager::Get());
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged), 0);

  // Arrow down to select an item checking the event has been emitted.
  DispatchKey(ui::VKEY_DOWN);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged), 1);

  DispatchKey(ui::VKEY_DOWN);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged), 2);
}

TEST_F(MenuControllerTest, AccessibilityEmitsMenuOpenedClosedEvents) {
  const test::AXEventCounter ax_counter(views::AXEventManager::Get());
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kMenuStart));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kMenuEnd));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd));

  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kMenuStart));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kMenuEnd));
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd));

  menu_controller()->Cancel(MenuController::ExitType::kAll);

  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kMenuStart));
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kMenuEnd));
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kMenuPopupStart));
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kMenuPopupEnd));
}

// Test that in accessibility mode disabled menu items are taken into account
// during items indices assignment.
TEST_F(MenuControllerTest, AccessibilityDisabledItemsIndices) {
  const ::ui::ScopedAXModeSetter ax_mode_setter(ui::AXMode::kNativeAPIs);

  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const MenuItemView* const item1 = submenu->GetMenuItemAt(0);
  MenuItemView* const item2 = submenu->GetMenuItemAt(1);
  const MenuItemView* const item3 = submenu->GetMenuItemAt(2);
  const MenuItemView* const item4 = submenu->GetMenuItemAt(3);

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
// This test exercises a Mac-specific behavior, by which hotkeys using
// modifiers cause menus to close and the hotkeys to be handled by the browser
// window. This specific test case tries using cmd-ctrl-f, which normally
// means "Fullscreen".
TEST_F(MenuControllerTest, BrowserHotkeysCancelMenusAndAreRedispatched) {
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  int options = ui::EF_COMMAND_DOWN;
  ui::KeyEvent press_cmd(ui::EventType::kKeyPressed, ui::VKEY_COMMAND, options);
  menu_controller()->OnWillDispatchKeyEvent(&press_cmd);
  EXPECT_TRUE(showing());  // ensure the command press itself doesn't cancel

  options |= ui::EF_CONTROL_DOWN;
  ui::KeyEvent press_ctrl(ui::EventType::kKeyPressed, ui::VKEY_CONTROL,
                          options);
  menu_controller()->OnWillDispatchKeyEvent(&press_ctrl);
  EXPECT_TRUE(showing());

  ui::KeyEvent press_f(ui::EventType::kKeyPressed, ui::VKEY_F, options);
  menu_controller()->OnWillDispatchKeyEvent(&press_f);
  EXPECT_FALSE(showing());
  EXPECT_FALSE(press_f.handled());
  EXPECT_FALSE(press_f.stopped_propagation());
}
#endif

TEST_F(MenuControllerTest, SubmenuOpenByKey) {
  // Create a submenu.
  MenuItemView* const child_menu = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  const SubmenuView* const submenu = child_menu->CreateSubmenu();
  child_menu->AppendMenuItem(5, u"Five");
  child_menu->AppendMenuItem(6, u"Six");

  // Open the menu and select the menu item that has a submenu.
  OpenMenu(menu_item());
  SetState(child_menu);
  EXPECT_EQ(1, pending_state_item()->GetCommand());
  EXPECT_EQ(nullptr, submenu->host());

  // Dispatch a key to open the submenu.
  DispatchKey(ui::VKEY_RIGHT);
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_NE(nullptr, submenu->host());
}

class ExecuteCommandWithoutClosingMenuTest : public MenuControllerTest {
 public:
  void SetUp() override {
    MenuControllerTest::SetUp();

    views::test::DisableMenuClosureAnimations();
    menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                           MenuAnchorPosition::kTopLeft, false, false);

    ShowSubmenu();

    menu_delegate()->set_should_execute_command_without_closing_menu(true);
  }
};

TEST_F(ExecuteCommandWithoutClosingMenuTest, OnClick) {
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());

  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const MenuItemView* const menu_item_view = submenu->GetMenuItemAt(0);
  const gfx::Point press_location = menu_item_view->bounds().CenterPoint();
  ProcessMousePressed(
      submenu, ui::MouseEvent(ui::EventType::kMousePressed, press_location,
                              press_location, ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON, 0));
  ProcessMouseReleased(
      submenu, ui::MouseEvent(ui::EventType::kMouseReleased, press_location,
                              press_location, ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON, 0));

  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_TRUE(showing());
  EXPECT_EQ(menu_delegate()->execute_command_id(),
            menu_item_view->GetCommand());
}

TEST_F(ExecuteCommandWithoutClosingMenuTest, OnTap) {
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());

  SubmenuView* const submenu = menu_item()->GetSubmenu();
  const MenuItemView* const menu_item_view = submenu->GetMenuItemAt(0);
  const gfx::Point tap_location = menu_item_view->bounds().CenterPoint();
  ProcessGestureEvent(
      submenu, ui::GestureEvent(
                   tap_location.x(), tap_location.y(), 0, ui::EventTimeForNow(),
                   ui::GestureEventDetails(ui::EventType::kGestureTap)));

  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_TRUE(showing());
  EXPECT_EQ(menu_delegate()->execute_command_id(),
            menu_item_view->GetCommand());
}

TEST_F(ExecuteCommandWithoutClosingMenuTest, OnReturnKey) {
  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());

  DispatchKey(ui::VKEY_DOWN);
  DispatchKey(ui::VKEY_RETURN);

  EXPECT_EQ(0, menu_controller_delegate()->on_menu_closed_called());
  EXPECT_TRUE(showing());
  EXPECT_EQ(menu_delegate()->execute_command_id(),
            menu_item()->GetSubmenu()->GetMenuItemAt(0)->GetCommand());
}

// Simple test to ensure child menu open direction is correctly set and
// retrieved.
TEST_F(MenuControllerTest, ChildMenuOpenDirectionStateUpdatesCorrectly) {
  // Before any open directions have been set, the leading direction should
  // be used as the default for any depth value.
  EXPECT_EQ(MenuController::MenuOpenDirection::kLeading,
            GetChildMenuOpenDirectionAtDepth(0));
  EXPECT_EQ(MenuController::MenuOpenDirection::kLeading,
            GetChildMenuOpenDirectionAtDepth(1));
  EXPECT_EQ(MenuController::MenuOpenDirection::kLeading,
            GetChildMenuOpenDirectionAtDepth(10));

  // Set alternating open directions, this should be correctly reflected in
  // subsequent open direction queries.
  SetChildMenuOpenDirectionAtDepth(1,
                                   MenuController::MenuOpenDirection::kLeading);
  SetChildMenuOpenDirectionAtDepth(
      2, MenuController::MenuOpenDirection::kTrailing);
  SetChildMenuOpenDirectionAtDepth(3,
                                   MenuController::MenuOpenDirection::kLeading);
  SetChildMenuOpenDirectionAtDepth(
      4, MenuController::MenuOpenDirection::kTrailing);

  EXPECT_EQ(MenuController::MenuOpenDirection::kLeading,
            GetChildMenuOpenDirectionAtDepth(0));
  EXPECT_EQ(MenuController::MenuOpenDirection::kLeading,
            GetChildMenuOpenDirectionAtDepth(1));
  EXPECT_EQ(MenuController::MenuOpenDirection::kTrailing,
            GetChildMenuOpenDirectionAtDepth(2));
  EXPECT_EQ(MenuController::MenuOpenDirection::kLeading,
            GetChildMenuOpenDirectionAtDepth(3));
  EXPECT_EQ(MenuController::MenuOpenDirection::kTrailing,
            GetChildMenuOpenDirectionAtDepth(4));
  EXPECT_EQ(MenuController::MenuOpenDirection::kLeading,
            GetChildMenuOpenDirectionAtDepth(10));
}

TEST_F(MenuControllerTest, MenuHostHasCorrectZOrderLevel) {
  ShowSubmenu();
  SubmenuView* const submenu = menu_item()->GetSubmenu();
  MenuHost* const host = menu_host_for_submenu(submenu);

  // Ensure that the menu host has the correct z order level.
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow, host->GetZOrderLevel());
}

}  // namespace views
