// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller.h"

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/buildflags.h"
#include "ui/views/corewm/test/tooltip_aura_test_api.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller_test_helper.h"
#include "ui/views/corewm/tooltip_state_manager.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/tooltip_observer.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

namespace views::corewm::test {
namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class TestTooltipLacros : public Tooltip {
 public:
  TestTooltipLacros() = default;

  TestTooltipLacros(const TestTooltipLacros&) = delete;
  TestTooltipLacros& operator=(const TestTooltipLacros&) = delete;

  ~TestTooltipLacros() override {
    state_manager_ = nullptr;
  }

  void AddObserver(wm::TooltipObserver* observer) override {}
  void RemoveObserver(wm::TooltipObserver* observer) override {}

  const std::u16string& tooltip_text() const { return tooltip_text_; }

  // Tooltip:
  int GetMaxWidth(const gfx::Point& location) const override { return 100; }
  void Update(aura::Window* window,
              const std::u16string& tooltip_text,
              const gfx::Point& position,
              const TooltipTrigger trigger) override {
    tooltip_parent_ = window;
    tooltip_text_ = tooltip_text;
    anchor_point_ = position + window->GetBoundsInScreen().OffsetFromOrigin();
    trigger_ = trigger;
  }
  void Show() override {
    is_visible_ = true;
    DCHECK(state_manager_);
    state_manager_->OnTooltipShownOnServer(tooltip_parent_, tooltip_text_,
                                           gfx::Rect());
  }
  void Hide() override {
    is_visible_ = false;
    tooltip_parent_ = nullptr;
    DCHECK(state_manager_);
    state_manager_->OnTooltipHiddenOnServer();
  }
  bool IsVisible() override { return is_visible_; }

  void SetStateManager(TooltipStateManager* state_manager) {
    state_manager_ = state_manager;
  }

  const gfx::Point& anchor_point() { return anchor_point_; }
  TooltipTrigger trigger() { return trigger_; }

 private:
  bool is_visible_ = false;
  raw_ptr<aura::Window> tooltip_parent_ = nullptr;
  raw_ptr<TooltipStateManager> state_manager_ = nullptr;  // not owned.
  std::u16string tooltip_text_;
  gfx::Point anchor_point_;
  TooltipTrigger trigger_;
};
#endif

std::unique_ptr<views::Widget> CreateWidget(aura::Window* root) {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
#if !BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_WIN)
  params.parent = root;
#endif
  params.bounds = gfx::Rect(0, 0, 200, 100);
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

}  // namespace

class TooltipControllerTest : public ViewsTestBase {
 public:
  TooltipControllerTest() = default;

  TooltipControllerTest(const TooltipControllerTest&) = delete;
  TooltipControllerTest& operator=(const TooltipControllerTest&) = delete;

  ~TooltipControllerTest() override = default;

  void SetUp() override {
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
    set_native_widget_type(NativeWidgetType::kDesktop);
#endif

    ViewsTestBase::SetUp();

    aura::Window* root_window = GetContext();
#if !BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_WIN)
    if (root_window) {
      tooltip_ = new views::corewm::TooltipAura();
      controller_ = std::make_unique<TooltipController>(
          std::unique_ptr<views::corewm::Tooltip>(tooltip_),
          /* activation_client */ nullptr);
      root_window->AddPreTargetHandler(controller_.get());
      SetTooltipClient(root_window, controller_.get());
    }
#endif
    widget_ = CreateWidget(root_window);
    widget_->SetContentsView(std::make_unique<View>());
    view_ = new TooltipTestView;
    widget_->GetContentsView()->AddChildView(view_.get());
    view_->SetBoundsRect(widget_->GetContentsView()->GetLocalBounds());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Use TestTooltip instead of TooltipLacros to avoid using server side
    // impl since it requires ui_controls which only works in
    // interactive_ui_tests.
    tooltip_ = new TestTooltipLacros();
    controller_ = std::make_unique<TooltipController>(
        std::unique_ptr<views::corewm::Tooltip>(tooltip_),
        /*activation_client=*/nullptr);
    widget_->GetNativeWindow()->GetRootWindow()->AddPreTargetHandler(
        controller_.get());
    // Set tooltip client after creating widget since tooltip controller is
    // constructed and overriden inside CreateWidget() for Lacros.
    SetTooltipClient(widget_->GetNativeWindow()->GetRootWindow(),
                     controller_.get());
#endif
    helper_ = std::make_unique<TooltipControllerTestHelper>(
        widget_->GetNativeWindow()->GetRootWindow());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    tooltip_->SetStateManager(helper_->state_manager());
#endif
    generator_ = std::make_unique<ui::test::EventGenerator>(GetRootWindow());
  }

  void TearDown() override {
    // Reset the tooltip in case tests end with a visible tooltip.
    helper_->state_manager()->HideAndReset();

#if !BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
    aura::Window* root_window = GetContext();
    if (root_window) {
      root_window->RemovePreTargetHandler(controller_.get());
      wm::SetTooltipClient(root_window, nullptr);
      tooltip_ = nullptr;
      helper_->set_controller(nullptr);
      controller_.reset();
    }
#endif
    generator_.reset();
    helper_.reset();
    view_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    widget_->CloseNow();
#endif
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  aura::Window* GetWindow() { return widget_->GetNativeWindow(); }

  aura::Window* GetRootWindow() { return GetWindow()->GetRootWindow(); }

  aura::Window* CreateNormalWindow(int id,
                                   aura::Window* parent,
                                   aura::WindowDelegate* delegate) {
    aura::Window* window = new aura::Window(
        delegate
            ? delegate
            : aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate());
    window->SetId(id);
    window->Init(ui::LAYER_TEXTURED);
    parent->AddChild(window);
    window->SetBounds(gfx::Rect(0, 0, 100, 100));
    window->Show();
    return window;
  }

  TooltipTestView* PrepareSecondView() {
    TooltipTestView* view2 = new TooltipTestView;
    widget_->GetContentsView()->AddChildView(view2);
    view_->SetBounds(0, 0, 100, 100);
    view2->SetBounds(100, 0, 100, 100);
    return view2;
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<TooltipTestView> view_ = nullptr;
  std::unique_ptr<TooltipControllerTestHelper> helper_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

 protected:
#if !BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_WIN)
  raw_ptr<TooltipAura> tooltip_;  // not owned.
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  raw_ptr<TestTooltipLacros> tooltip_;  // not owned.
#endif

 private:
  std::unique_ptr<TooltipController> controller_;

#if BUILDFLAG(IS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif
};

TEST_F(TooltipControllerTest, ViewTooltip) {
  view_->set_tooltip_text(u"Tooltip Text");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  generator_->MoveMouseToCenterOf(GetWindow());

  EXPECT_EQ(GetWindow(), GetRootWindow()->GetEventHandlerForPoint(
                             generator_->current_screen_location()));
  std::u16string expected_tooltip = u"Tooltip Text";
  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  generator_->MoveMouseBy(1, 0);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());
}

TEST_F(TooltipControllerTest, HideEmptyTooltip) {
  view_->set_tooltip_text(u"Tooltip Text");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  generator_->MoveMouseToCenterOf(GetWindow());
  generator_->MoveMouseBy(1, 0);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);

  view_->set_tooltip_text(u"    ");
  generator_->MoveMouseBy(1, 0);
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, DontShowTooltipOnTouch) {
  view_->set_tooltip_text(u"Tooltip Text");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  generator_->PressMoveAndReleaseTouchToCenterOf(GetWindow());
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  generator_->MoveMouseToCenterOf(GetWindow());
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  generator_->MoveMouseBy(1, 0);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  std::u16string expected_tooltip = u"Tooltip Text";
  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());
}

#if !BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_WIN)
// crbug.com/664370.
TEST_F(TooltipControllerTest, MaxWidth) {
  std::u16string text =
      u"Really, really, really, really, really, really long tooltip that "
      u"exceeds max width";
  view_->set_tooltip_text(text);
  gfx::Point center = GetWindow()->bounds().CenterPoint();

  generator_->MoveMouseTo(center);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  const gfx::RenderText* render_text =
      test::TooltipAuraTestApi(tooltip_).GetRenderText();

  int max = helper_->controller()->GetMaxWidth(center);
  EXPECT_EQ(max, render_text->display_rect().width());
}

TEST_F(TooltipControllerTest, AccessibleNodeData) {
  std::u16string text = u"Tooltip Text";
  view_->set_tooltip_text(text);
  gfx::Point center = GetWindow()->bounds().CenterPoint();

  generator_->MoveMouseTo(center);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  ui::AXNodeData node_data;
  test::TooltipAuraTestApi(tooltip_).GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kTooltip, node_data.role);
  EXPECT_EQ(text, base::ASCIIToUTF16(node_data.GetStringAttribute(
                      ax::mojom::StringAttribute::kName)));
}

TEST_F(TooltipControllerTest, TooltipBounds) {
  // We don't need a real tootip. Let's just use a custom size and custom point
  // to test this function.
  gfx::Size tooltip_size(100, 40);
  gfx::Rect display_bounds(display::Screen::GetScreen()
                               ->GetDisplayNearestPoint(gfx::Point(0, 0))
                               .bounds());
  gfx::Point anchor_point = display_bounds.CenterPoint();

  // All tests here share the same expected y value.
  int a_expected_y(anchor_point.y() + TooltipAura::kCursorOffsetY);
  int b_expected_y(anchor_point.y());

  // 1. The tooltip fits entirely in the window.
  {
    // A. When attached to the cursor, the tooltip should be positioned at the
    // bottom-right corner of the cursor.
    gfx::Rect bounds = test::TooltipAuraTestApi(tooltip_).GetTooltipBounds(
        tooltip_size, anchor_point, TooltipTrigger::kCursor);
    gfx::Point expected_position(anchor_point.x() + TooltipAura::kCursorOffsetX,
                                 a_expected_y);
    EXPECT_EQ(bounds, gfx::Rect(expected_position, tooltip_size));

    // B. When not attached to the cursor, the tooltip should be horizontally
    // centered with the anchor point.
    bounds = test::TooltipAuraTestApi(tooltip_).GetTooltipBounds(
        tooltip_size, anchor_point, TooltipTrigger::kKeyboard);
    expected_position =
        gfx::Point(anchor_point.x() - tooltip_size.width() / 2, b_expected_y);
    EXPECT_EQ(bounds, gfx::Rect(expected_position, tooltip_size));
  }
  // 2. The tooltip overflows on the left side of the window.
  {
    anchor_point = display_bounds.left_center();
    anchor_point.Offset(-TooltipAura::kCursorOffsetX - 10, 0);

    // A. When attached to the cursor, the tooltip should be positioned at the
    // bottom-right corner of the cursor.
    gfx::Rect bounds = test::TooltipAuraTestApi(tooltip_).GetTooltipBounds(
        tooltip_size, anchor_point, TooltipTrigger::kCursor);
    gfx::Point expected_position(0, a_expected_y);
    EXPECT_EQ(bounds, gfx::Rect(expected_position, tooltip_size));

    // B. When not attached to the cursor, the tooltip should be horizontally
    // centered with the anchor point.
    bounds = test::TooltipAuraTestApi(tooltip_).GetTooltipBounds(
        tooltip_size, anchor_point, TooltipTrigger::kKeyboard);
    expected_position = gfx::Point(0, b_expected_y);
    EXPECT_EQ(bounds, gfx::Rect(expected_position, tooltip_size));
  }
  // 3. The tooltip overflows on the right side of the window.
  {
    anchor_point = display_bounds.right_center();
    anchor_point.Offset(10, 0);

    // A. When attached to the cursor, the tooltip should be positioned at the
    // bottom-right corner of the cursor.
    gfx::Rect bounds = test::TooltipAuraTestApi(tooltip_).GetTooltipBounds(
        tooltip_size, anchor_point, TooltipTrigger::kCursor);
    gfx::Point expected_position(display_bounds.right() - tooltip_size.width(),
                                 a_expected_y);
    EXPECT_EQ(bounds, gfx::Rect(expected_position, tooltip_size));

    // B. When not attached to the cursor, the tooltip should be horizontally
    // centered with the anchor point.
    bounds = test::TooltipAuraTestApi(tooltip_).GetTooltipBounds(
        tooltip_size, anchor_point, TooltipTrigger::kKeyboard);
    expected_position =
        gfx::Point(display_bounds.right() - tooltip_size.width(), b_expected_y);
    EXPECT_EQ(bounds, gfx::Rect(expected_position, tooltip_size));
  }
  // 4. The tooltip overflows on the bottom.
  {
    anchor_point = display_bounds.bottom_center();

    // A. When attached to the cursor, the tooltip should be positioned at the
    // bottom-right corner of the cursor.
    gfx::Rect bounds = test::TooltipAuraTestApi(tooltip_).GetTooltipBounds(
        tooltip_size, anchor_point, TooltipTrigger::kCursor);
    gfx::Point expected_position(anchor_point.x() + TooltipAura::kCursorOffsetX,
                                 anchor_point.y() - tooltip_size.height());
    EXPECT_EQ(bounds, gfx::Rect(expected_position, tooltip_size));

    // B. When not attached to the cursor, the tooltip should be horizontally
    // centered with the anchor point.
    bounds = test::TooltipAuraTestApi(tooltip_).GetTooltipBounds(
        tooltip_size, anchor_point, TooltipTrigger::kKeyboard);
    expected_position = gfx::Point(anchor_point.x() - tooltip_size.width() / 2,
                                   anchor_point.y() - tooltip_size.height());
    EXPECT_EQ(bounds, gfx::Rect(expected_position, tooltip_size));
  }
}
#endif

TEST_F(TooltipControllerTest, TooltipsInMultipleViews) {
  view_->set_tooltip_text(u"Tooltip Text");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  PrepareSecondView();
  aura::Window* window = GetWindow();
  aura::Window* root_window = GetRootWindow();

  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_TRUE(helper_->IsTooltipVisible());
    EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
              TooltipTrigger::kCursor);
    EXPECT_EQ(window, root_window->GetEventHandlerForPoint(
                          generator_->current_screen_location()));
    std::u16string expected_tooltip = u"Tooltip Text";
    EXPECT_EQ(expected_tooltip, wm::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipParentWindow());
  }
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_EQ(window, root_window->GetEventHandlerForPoint(
                          generator_->current_screen_location()));
    std::u16string expected_tooltip;  // = ""
    EXPECT_EQ(expected_tooltip, wm::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipParentWindow());
  }
}

TEST_F(TooltipControllerTest, EnableOrDisableTooltips) {
  view_->set_tooltip_text(u"Tooltip Text");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  generator_->MoveMouseRelativeTo(GetWindow(), view_->bounds().CenterPoint());
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);

  // Disable tooltips and check again.
  helper_->controller()->SetTooltipsEnabled(false);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  helper_->UpdateIfRequired(TooltipTrigger::kCursor);
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // Enable tooltips back and check again.
  helper_->controller()->SetTooltipsEnabled(true);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  helper_->UpdateIfRequired(TooltipTrigger::kCursor);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
}

// Verifies tooltip isn't shown if tooltip text consists entirely of whitespace.
TEST_F(TooltipControllerTest, DontShowEmptyTooltips) {
  view_->set_tooltip_text(u"                     ");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  generator_->MoveMouseRelativeTo(GetWindow(), view_->bounds().CenterPoint());
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

// Disabled on Lacros since TooltipLacros does not have tooltip timer on client
// side so cannot be tested on unittest.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TooltipUpdateWhenTooltipDeferTimerIsRunning \
  DISABLED_TooltipUpdateWhenTooltipDeferTimerIsRunning
#else
#define MAYBE_TooltipUpdateWhenTooltipDeferTimerIsRunning \
  TooltipUpdateWhenTooltipDeferTimerIsRunning
#endif
TEST_F(TooltipControllerTest,
       MAYBE_TooltipUpdateWhenTooltipDeferTimerIsRunning) {
  view_->set_tooltip_text(u"Tooltip Text for view 1");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  TooltipTestView* view2 = PrepareSecondView();
  view2->set_tooltip_text(u"Tooltip Text for view 2");

  aura::Window* window = GetWindow();

  // Tooltips show up with delay
  helper_->SkipTooltipShowDelay(false);

  // Tooltip 1 is scheduled and invisibled
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_FALSE(helper_->IsWillHideTooltipTimerRunning());

  // Tooltip 2 is scheduled and invisible, the expected tooltip is tooltip 2
  generator_->MoveMouseRelativeTo(window, view2->bounds().CenterPoint());
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_FALSE(helper_->IsWillHideTooltipTimerRunning());
  std::u16string expected_tooltip = u"Tooltip Text for view 2";
  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(window, helper_->GetTooltipParentWindow());

  helper_->SkipTooltipShowDelay(true);
}

TEST_F(TooltipControllerTest, TooltipHidesOnKeyPressAndStaysHiddenUntilChange) {
  view_->set_tooltip_text(u"Tooltip Text for view 1");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  TooltipTestView* view2 = PrepareSecondView();
  view2->set_tooltip_text(u"Tooltip Text for view 2");

  aura::Window* window = GetWindow();

  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_TRUE(helper_->IsWillHideTooltipTimerRunning());
#endif

  generator_->PressKey(ui::VKEY_1, 0);
  EXPECT_FALSE(helper_->IsTooltipVisible());
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(helper_->IsWillHideTooltipTimerRunning());
#endif

  // Moving the mouse inside |view1| should not change the state of the tooltip
  // or the timers.
  for (int i = 0; i < 49; i++) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    EXPECT_FALSE(helper_->IsWillHideTooltipTimerRunning());
#endif
    EXPECT_EQ(window, GetRootWindow()->GetEventHandlerForPoint(
                          generator_->current_screen_location()));
    std::u16string expected_tooltip = u"Tooltip Text for view 1";
    EXPECT_EQ(expected_tooltip, wm::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetObservedWindow());
  }

  // Now we move the mouse on to |view2|. It should update the tooltip.
  generator_->MoveMouseBy(1, 0);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_TRUE(helper_->IsWillHideTooltipTimerRunning());
#endif
  std::u16string expected_tooltip = u"Tooltip Text for view 2";
  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(window, helper_->GetTooltipParentWindow());
}

TEST_F(TooltipControllerTest, TooltipStaysVisibleOnKeyRelease) {
  view_->set_tooltip_text(u"my tooltip");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  generator_->MoveMouseRelativeTo(GetWindow(), view_->bounds().CenterPoint());
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // This shouldn't hide the tooltip.
  generator_->ReleaseKey(ui::VKEY_1, 0);
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // This should hide the tooltip.
  generator_->PressKey(ui::VKEY_1, 0);
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, TooltipHidesOnTimeoutAndStaysHiddenUntilChange) {
  view_->set_tooltip_text(u"Tooltip Text for view 1");
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  TooltipTestView* view2 = PrepareSecondView();
  view2->set_tooltip_text(u"Tooltip Text for view 2");

  aura::Window* window = GetWindow();

  // Update tooltip so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_TRUE(helper_->IsWillHideTooltipTimerRunning());
#endif

  helper_->FireHideTooltipTimer();
  EXPECT_FALSE(helper_->IsTooltipVisible());
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(helper_->IsWillHideTooltipTimerRunning());
#endif

  // Moving the mouse inside |view1| should not change the state of the tooltip
  // or the timers.
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    EXPECT_FALSE(helper_->IsWillHideTooltipTimerRunning());
#endif
    EXPECT_EQ(window, GetRootWindow()->GetEventHandlerForPoint(
                          generator_->current_screen_location()));
    std::u16string expected_tooltip = u"Tooltip Text for view 1";
    EXPECT_EQ(expected_tooltip, wm::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetObservedWindow());
  }

  // Now we move the mouse on to |view2|. It should update the tooltip.
  generator_->MoveMouseBy(1, 0);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_TRUE(helper_->IsWillHideTooltipTimerRunning());
#endif
  std::u16string expected_tooltip = u"Tooltip Text for view 2";
  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(window, helper_->GetTooltipParentWindow());
}

// Verifies a mouse exit event hides the tooltips.
TEST_F(TooltipControllerTest, HideOnExit) {
  view_->set_tooltip_text(u"Tooltip Text");
  generator_->MoveMouseToCenterOf(GetWindow());
  std::u16string expected_tooltip = u"Tooltip Text";
  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  generator_->SendMouseExit();
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, ReshowOnClickAfterEnterExit) {
  // Owned by |view_|.
  TooltipTestView* v1 = new TooltipTestView;
  TooltipTestView* v2 = new TooltipTestView;
  view_->AddChildView(v1);
  view_->AddChildView(v2);
  gfx::Rect view_bounds(view_->GetLocalBounds());
  view_bounds.set_height(view_bounds.height() / 2);
  v1->SetBoundsRect(view_bounds);
  view_bounds.set_y(view_bounds.height());
  v2->SetBoundsRect(view_bounds);
  const std::u16string v1_tt(u"v1");
  const std::u16string v2_tt(u"v2");
  v1->set_tooltip_text(v1_tt);
  v2->set_tooltip_text(v2_tt);

  gfx::Point v1_point(1, 1);
  View::ConvertPointToWidget(v1, &v1_point);
  generator_->MoveMouseRelativeTo(GetWindow(), v1_point);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  EXPECT_EQ(v1_tt, helper_->GetTooltipText());

  // Press the mouse, move to v2 and back to v1.
  generator_->ClickLeftButton();

  gfx::Point v2_point(1, 1);
  View::ConvertPointToWidget(v2, &v2_point);
  generator_->MoveMouseRelativeTo(GetWindow(), v2_point);
  generator_->MoveMouseRelativeTo(GetWindow(), v1_point);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  EXPECT_EQ(v1_tt, helper_->GetTooltipText());
}

TEST_F(TooltipControllerTest, ShowAndHideTooltipTriggeredFromKeyboard) {
  std::u16string expected_tooltip = u"Tooltip Text";

  wm::SetTooltipText(GetWindow(), &expected_tooltip);
  view_->set_tooltip_text(expected_tooltip);
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  helper_->controller()->UpdateTooltipFromKeyboard(
      view_->ConvertRectToWidget(view_->bounds()), GetWindow());

  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kKeyboard);

  helper_->HideAndReset();

  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());
}

TEST_F(TooltipControllerTest,
       KeyboardTriggeredTooltipStaysVisibleOnMouseExitedEvent) {
  std::u16string expected_tooltip = u"Tooltip Text";

  wm::SetTooltipText(GetWindow(), &expected_tooltip);
  view_->set_tooltip_text(expected_tooltip);
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());

  // For this test to execute properly, make sure that the cursor location is
  // somewhere out of the |view_|, different than (0, 0). This shouldn't show
  // the tooltip.
  gfx::Point off_view_point = view_->bounds().bottom_right();
  off_view_point.Offset(1, 1);
  generator_->MoveMouseRelativeTo(widget_->GetNativeWindow(), off_view_point);
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // Trigger the tooltip from the keyboard.
  helper_->controller()->UpdateTooltipFromKeyboard(
      view_->ConvertRectToWidget(view_->bounds()), GetWindow());

  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kKeyboard);

  // Sending a mouse exited event shouldn't hide a keyboard triggered tooltip.
  generator_->SendMouseExit();
  EXPECT_TRUE(helper_->IsTooltipVisible());

  helper_->HideAndReset();
  expected_tooltip = u"Tooltip Text 2";
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // However, a cursor triggered tooltip should still be hidden by a mouse
  // exited event.
  generator_->MoveMouseRelativeTo(widget_->GetNativeWindow(),
                                  view_->bounds().CenterPoint());
  EXPECT_TRUE(helper_->IsTooltipVisible());

  generator_->SendMouseExit();
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

namespace {

// Returns the index of |window| in its parent's children.
int IndexInParent(const aura::Window* window) {
  auto i = base::ranges::find(window->parent()->children(), window);
  return i == window->parent()->children().end()
             ? -1
             : static_cast<int>(i - window->parent()->children().begin());
}

}  // namespace

// Verifies when capture is released the TooltipController resets state.
// Flaky on all builders.  http://crbug.com/388268
TEST_F(TooltipControllerTest, DISABLED_CloseOnCaptureLost) {
  view_->GetWidget()->SetCapture(view_);
  RunPendingMessages();
  view_->set_tooltip_text(u"Tooltip Text");
  generator_->MoveMouseToCenterOf(GetWindow());
  std::u16string expected_tooltip = u"Tooltip Text";
  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(GetWindow()));
  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  view_->GetWidget()->ReleaseCapture();
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->GetTooltipParentWindow() == nullptr);
}

// Disabled on Linux as X11ScreenOzone::GetAcceleratedWidgetAtScreenPoint
// and WaylandScreen::GetAcceleratedWidgetAtScreenPoint don't consider z-order.
// Disabled on Windows due to failing bots. http://crbug.com/604479
// Disabled on Lacros due to crash and flakiness.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif
// Verifies the correct window is found for tooltips when there is a capture.
TEST_F(TooltipControllerTest, MAYBE_Capture) {
  const std::u16string tooltip_text(u"1");
  const std::u16string tooltip_text2(u"2");
  const std::u16string tooltip_text_child(u"child");

  widget_->SetBounds(gfx::Rect(0, 0, 200, 200));
  view_->set_tooltip_text(tooltip_text);

  std::unique_ptr<views::Widget> widget2 = CreateWidget(GetContext());
  widget2->SetContentsView(std::make_unique<View>());
  TooltipTestView* view2 = new TooltipTestView;
  widget2->GetContentsView()->AddChildView(view2);
  view2->set_tooltip_text(tooltip_text2);
  widget2->SetBounds(gfx::Rect(0, 0, 200, 200));
  view2->SetBoundsRect(widget2->GetContentsView()->GetLocalBounds());

  widget_->SetCapture(view_);
  EXPECT_TRUE(widget_->HasCapture());
  widget2->Show();
  EXPECT_GE(IndexInParent(widget2->GetNativeWindow()),
            IndexInParent(widget_->GetNativeWindow()));

  generator_->MoveMouseRelativeTo(widget_->GetNativeWindow(),
                                  view_->bounds().CenterPoint());

  // Even though the mouse is over a window with a tooltip it shouldn't be
  // picked up because the windows don't have the same value for
  // |TooltipManager::kGroupingPropertyKey|.
  EXPECT_TRUE(helper_->GetTooltipText().empty());

  // Now make both the windows have same transient value for
  // kGroupingPropertyKey. In this case the tooltip should be picked up from
  // |widget2| (because the mouse is over it).
  const int grouping_key = 1;
  widget_->SetNativeWindowProperty(TooltipManager::kGroupingPropertyKey,
                                   reinterpret_cast<void*>(grouping_key));
  widget2->SetNativeWindowProperty(TooltipManager::kGroupingPropertyKey,
                                   reinterpret_cast<void*>(grouping_key));
  generator_->MoveMouseBy(1, 10);
  EXPECT_EQ(tooltip_text2, helper_->GetTooltipText());

  // Make child widget under widget2 and let the mouse be over the child widget.
  // Even though the child widget does not have grouping property key, it should
  // refer to its parent property. In this scenario, `widget_child`'s parent is
  // `widget2` and it has the same kGroupingPropertyKey as `widget_`'s key, so
  // `widget_child` should show tooltip when `widget_` has a capture.
  std::unique_ptr<views::Widget> widget_child = CreateWidget(GetContext());
  widget_child->SetContentsView(std::make_unique<View>());
  TooltipTestView* view_child = new TooltipTestView;
  widget_child->GetContentsView()->AddChildView(view_child);
  view_child->set_tooltip_text(tooltip_text_child);
  widget_child->SetBounds(gfx::Rect(0, 0, 200, 200));
  view_child->SetBoundsRect(widget_child->GetContentsView()->GetLocalBounds());
  Widget::ReparentNativeView(widget_child->GetNativeView(),
                             widget2->GetNativeView());
  widget_child->Show();

  generator_->MoveMouseBy(1, 10);
  EXPECT_EQ(tooltip_text_child, helper_->GetTooltipText());

  widget2.reset();
}

TEST_F(TooltipControllerTest, ShowTooltipOnTooltipTextUpdate) {
  std::u16string expected_tooltip;

  wm::SetTooltipText(GetWindow(), &expected_tooltip);

  // Create a mouse event. This event shouldn't trigger the tooltip to show
  // since the tooltip text is empty, but should set the |observed_window_|
  // correctly.
  gfx::Point point(1, 1);
  View::ConvertPointToWidget(view_, &point);
  generator_->MoveMouseRelativeTo(GetWindow(), point);

  EXPECT_EQ(std::u16string(), helper_->GetTooltipText());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());
  EXPECT_EQ(GetWindow(), helper_->GetObservedWindow());
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // This is the heart of the test: we update the tooltip text and call
  // UpdateTooltip. It should trigger the tooltip to show up because the
  // |observed_window_| will be set to GetWindow() and the tooltip text on the
  // window will be different than it previously was.
  expected_tooltip = u"Tooltip text";
  helper_->controller()->UpdateTooltip(GetWindow());

  EXPECT_EQ(expected_tooltip, wm::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);

  helper_->HideAndReset();

  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_EQ(nullptr, helper_->GetTooltipParentWindow());
}

// Disabled on Lacros since TooltipLacros does not have tooltip timer on client
// side so cannot be tested on unittest.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TooltipPositionUpdatedWhenTimerRunning \
  DISABLED_TooltipPositionUpdatedWhenTimerRunning
#else
#define MAYBE_TooltipPositionUpdatedWhenTimerRunning \
  TooltipPositionUpdatedWhenTimerRunning
#endif
// This test validates that the TooltipController correctly triggers a position
// update for a tooltip that is about to be shown.
TEST_F(TooltipControllerTest, MAYBE_TooltipPositionUpdatedWhenTimerRunning) {
  EXPECT_EQ(nullptr, helper_->state_manager()->tooltip_parent_window());
  EXPECT_EQ(std::u16string(), helper_->state_manager()->tooltip_text());

  std::u16string expected_text = u"Tooltip Text";
  view_->set_tooltip_text(expected_text);

  helper_->SkipTooltipShowDelay(false);

  // Testing that the position will be updated when triggered from cursor.
  {
    gfx::Point position = view_->bounds().CenterPoint();
    generator_->MoveMouseRelativeTo(GetWindow(), position);

    EXPECT_EQ(expected_text, wm::GetTooltipText(GetWindow()));
    EXPECT_EQ(expected_text, helper_->GetTooltipText());
    EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());
    EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
              TooltipTrigger::kCursor);
    EXPECT_EQ(position, helper_->GetTooltipPosition());

    // Since the |will_show_tooltip_timer_| is running, this should update the
    // position of the already active tooltip.
    generator_->MoveMouseBy(2, 0);

    position.Offset(2, 0);
    EXPECT_EQ(position, helper_->GetTooltipPosition());

    helper_->HideAndReset();
  }

  // Testing that the position will be updated when triggered from cursor.
  {
    gfx::Rect bounds = view_->ConvertRectToWidget(view_->bounds());
    helper_->controller()->UpdateTooltipFromKeyboard(bounds, GetWindow());

    EXPECT_EQ(expected_text, wm::GetTooltipText(GetWindow()));
    EXPECT_EQ(expected_text, helper_->GetTooltipText());
    EXPECT_EQ(GetWindow(), helper_->GetTooltipParentWindow());
    EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
              TooltipTrigger::kKeyboard);
    EXPECT_EQ(bounds.bottom_center(), helper_->GetTooltipPosition());

    // Since the |will_show_tooltip_timer_| is running, this should update the
    // position of the already active tooltip.
    bounds.Offset(2, 0);
    helper_->controller()->UpdateTooltipFromKeyboard(bounds, GetWindow());

    EXPECT_EQ(bounds.bottom_center(), helper_->GetTooltipPosition());

    helper_->HideAndReset();
  }

  helper_->SkipTooltipShowDelay(true);
}

// This test validates that tooltips are hidden when the currently active window
// loses focus to another window.
TEST_F(TooltipControllerTest, TooltipHiddenWhenWindowDeactivated) {
  EXPECT_EQ(nullptr, helper_->state_manager()->tooltip_parent_window());
  EXPECT_EQ(std::u16string(), helper_->state_manager()->tooltip_text());

  view_->set_tooltip_text(u"Tooltip text 1");

  // Start by showing the tooltip.
  gfx::Point in_view_point = view_->bounds().CenterPoint();
  generator_->MoveMouseRelativeTo(GetWindow(), in_view_point);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);

  // Then mock a window deactivation event.
  helper_->MockWindowActivated(GetWindow(), /* active */ false);

  // The previously visible tooltip should have been closed by that event.
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // The tooltip should show up again if we move the cursor again.
  view_->set_tooltip_text(u"Tooltip text 2");
  generator_->MoveMouseBy(1, 1);
  EXPECT_TRUE(helper_->IsTooltipVisible());
}

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
using TestTooltip = TestTooltipLacros;
#else
class TestTooltip : public Tooltip {
 public:
  TestTooltip() = default;

  TestTooltip(const TestTooltip&) = delete;
  TestTooltip& operator=(const TestTooltip&) = delete;

  ~TestTooltip() override = default;

  void AddObserver(wm::TooltipObserver* observer) override {}
  void RemoveObserver(wm::TooltipObserver* observer) override {}

  const std::u16string& tooltip_text() const { return tooltip_text_; }

  // Tooltip:
  int GetMaxWidth(const gfx::Point& location) const override { return 100; }
  void Update(aura::Window* window,
              const std::u16string& tooltip_text,
              const gfx::Point& position,
              const TooltipTrigger trigger) override {
    tooltip_text_ = tooltip_text;
    anchor_point_ = position + window->GetBoundsInScreen().OffsetFromOrigin();
    trigger_ = trigger;
  }
  void Show() override { is_visible_ = true; }
  void Hide() override { is_visible_ = false; }
  bool IsVisible() override { return is_visible_; }
  const gfx::Point& anchor_point() { return anchor_point_; }
  TooltipTrigger trigger() { return trigger_; }

 private:
  bool is_visible_ = false;
  std::u16string tooltip_text_;
  gfx::Point anchor_point_;
  TooltipTrigger trigger_;
};
#endif

}  // namespace

// Use for tests that don't depend upon views.
class TooltipControllerTest2 : public aura::test::AuraTestBase {
 public:
  TooltipControllerTest2() : test_tooltip_(new TestTooltip) {}

  TooltipControllerTest2(const TooltipControllerTest2&) = delete;
  TooltipControllerTest2& operator=(const TooltipControllerTest2&) = delete;

  ~TooltipControllerTest2() override = default;

  void SetUp() override {
    at_exit_manager_ = std::make_unique<base::ShadowingAtExitManager>();
    aura::test::AuraTestBase::SetUp();
    controller_ = std::make_unique<TooltipController>(
        std::unique_ptr<corewm::Tooltip>(test_tooltip_),
        /* activation_client */ nullptr);
    root_window()->AddPreTargetHandler(controller_.get());
    SetTooltipClient(root_window(), controller_.get());
    helper_ = std::make_unique<TooltipControllerTestHelper>(root_window());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    test_tooltip_->SetStateManager(helper_->state_manager());
#endif
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window());
  }

  void TearDown() override {
    // Reset the tooltip in case tests end with a visible tooltip.
    helper_->state_manager()->HideAndReset();

    root_window()->RemovePreTargetHandler(controller_.get());
    wm::SetTooltipClient(root_window(), nullptr);
    controller_.reset();
    generator_.reset();
    helper_.reset();
    aura::test::AuraTestBase::TearDown();
    at_exit_manager_.reset();
  }

 protected:
  // Owned by |controller_|.
  raw_ptr<TestTooltip, DanglingUntriaged> test_tooltip_;
  std::unique_ptr<TooltipControllerTestHelper> helper_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

 private:
  // Needed to make sure the DeviceDataManager is cleaned up between test runs.
  std::unique_ptr<base::ShadowingAtExitManager> at_exit_manager_;
  std::unique_ptr<TooltipController> controller_;
};

TEST_F(TooltipControllerTest2, VerifyLeadingTrailingWhitespaceStripped) {
  aura::test::TestWindowDelegate test_delegate;
  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(100, root_window(), &test_delegate));
  window->SetBounds(gfx::Rect(0, 0, 300, 300));
  std::u16string tooltip_text(u" \nx  ");
  wm::SetTooltipText(window.get(), &tooltip_text);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  generator_->MoveMouseToCenterOf(window.get());
  EXPECT_EQ(u"x", test_tooltip_->tooltip_text());
}

// Verifies that tooltip is hidden and tooltip window closed upon cancel mode.
TEST_F(TooltipControllerTest2, CloseOnCancelMode) {
  aura::test::TestWindowDelegate test_delegate;
  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(100, root_window(), &test_delegate));
  window->SetBounds(gfx::Rect(0, 0, 300, 300));
  std::u16string tooltip_text(u"Tooltip Text");
  wm::SetTooltipText(window.get(), &tooltip_text);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  generator_->MoveMouseToCenterOf(window.get());

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);

  // Send OnCancelMode event and verify that tooltip becomes invisible and
  // the tooltip window is closed.
  ui::CancelModeEvent event;
  helper_->controller()->OnCancelMode(&event);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->GetTooltipParentWindow() == nullptr);
}

// Use for tests that need both views and a TestTooltip.
class TooltipControllerTest3 : public ViewsTestBase {
 public:
  TooltipControllerTest3() = default;

  TooltipControllerTest3(const TooltipControllerTest3&) = delete;
  TooltipControllerTest3& operator=(const TooltipControllerTest3&) = delete;

  ~TooltipControllerTest3() override = default;

  void SetUp() override {
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
    set_native_widget_type(NativeWidgetType::kDesktop);
#endif

    ViewsTestBase::SetUp();

    widget_ = CreateWidget(GetContext());
    widget_->SetContentsView(std::make_unique<View>());
    view_ = new TooltipTestView;
    widget_->GetContentsView()->AddChildView(view_.get());
    view_->SetBoundsRect(widget_->GetContentsView()->GetLocalBounds());

    generator_ = std::make_unique<ui::test::EventGenerator>(GetRootWindow());
    auto tooltip = std::make_unique<TestTooltip>();
    test_tooltip_ = tooltip.get();
    controller_ = std::make_unique<TooltipController>(
        std::move(tooltip), /* activation_client */ nullptr);
    auto* tooltip_controller =
        static_cast<TooltipController*>(wm::GetTooltipClient(GetRootWindow()));
    if (tooltip_controller)
      GetRootWindow()->RemovePreTargetHandler(tooltip_controller);
    GetRootWindow()->AddPreTargetHandler(controller_.get());
    SetTooltipClient(GetRootWindow(), controller_.get());
    helper_ = std::make_unique<TooltipControllerTestHelper>(GetRootWindow());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    test_tooltip_->SetStateManager(helper_->state_manager());
#endif
  }

  void TearDown() override {
    GetRootWindow()->RemovePreTargetHandler(controller_.get());
    wm::SetTooltipClient(GetRootWindow(), nullptr);

    controller_.reset();
    generator_.reset();
    helper_.reset();
    view_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  aura::Window* GetWindow() { return widget_->GetNativeWindow(); }

 protected:
  // Owned by |controller_|.
  raw_ptr<TestTooltip, DanglingUntriaged> test_tooltip_ = nullptr;
  std::unique_ptr<TooltipControllerTestHelper> helper_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<TooltipTestView> view_;

 private:
  std::unique_ptr<TooltipController> controller_;

#if BUILDFLAG(IS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  aura::Window* GetRootWindow() { return GetWindow()->GetRootWindow(); }
};

TEST_F(TooltipControllerTest3, TooltipPositionChangesOnTwoViewsWithSameLabel) {
  // Owned by |view_|.
  // These two views have the same tooltip text
  TooltipTestView* v1 = new TooltipTestView;
  TooltipTestView* v2 = new TooltipTestView;
  // v1_1 is a view inside v1 that has an identical tooltip text to that of v1
  // and v2
  TooltipTestView* v1_1 = new TooltipTestView;
  // v2_1 is a view inside v2 that has an identical tooltip text to that of v1
  // and v2
  TooltipTestView* v2_1 = new TooltipTestView;
  // v2_2 is a view inside v2 with the tooltip text different from all the
  // others
  TooltipTestView* v2_2 = new TooltipTestView;

  // Setup all the views' relations
  view_->AddChildView(v1);
  view_->AddChildView(v2);
  v1->AddChildView(v1_1);
  v2->AddChildView(v2_1);
  v2->AddChildView(v2_2);
  const std::u16string reference_string(u"Identical Tooltip Text");
  const std::u16string alternative_string(u"Another Shrubbery");
  v1->set_tooltip_text(reference_string);
  v2->set_tooltip_text(reference_string);
  v1_1->set_tooltip_text(reference_string);
  v2_1->set_tooltip_text(reference_string);
  v2_2->set_tooltip_text(alternative_string);

  // Set views' bounds
  gfx::Rect view_bounds(view_->GetLocalBounds());
  view_bounds.set_height(view_bounds.height() / 2);
  v1->SetBoundsRect(view_bounds);
  v1_1->SetBounds(0, 0, 3, 3);
  view_bounds.set_y(view_bounds.height());
  v2->SetBoundsRect(view_bounds);
  v2_2->SetBounds(view_bounds.width() - 3, view_bounds.height() - 3, 3, 3);
  v2_1->SetBounds(0, 0, 3, 3);

  // Test whether a toolbar appears on v1
  gfx::Point center = v1->bounds().CenterPoint();
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  EXPECT_EQ(reference_string, helper_->GetTooltipText());
  gfx::Point tooltip_bounds1 = test_tooltip_->anchor_point();

  // Test whether the toolbar changes position on mouse over v2
  center = v2->bounds().CenterPoint();
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  EXPECT_EQ(reference_string, helper_->GetTooltipText());
  gfx::Point tooltip_bounds2 = test_tooltip_->anchor_point();

  EXPECT_NE(tooltip_bounds1, gfx::Point());
  EXPECT_NE(tooltip_bounds2, gfx::Point());
  EXPECT_NE(tooltip_bounds1, tooltip_bounds2);

  // Test if the toolbar does not change position on encountering a contained
  // view with the same tooltip text
  center = v2_1->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(v2_1, view_, &center);
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  gfx::Point tooltip_bounds2_1 = test_tooltip_->anchor_point();

  EXPECT_NE(tooltip_bounds2, tooltip_bounds2_1);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  EXPECT_EQ(reference_string, helper_->GetTooltipText());

  // Test if the toolbar changes position on encountering a contained
  // view with a different tooltip text
  center = v2_2->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(v2_2, view_, &center);
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  gfx::Point tooltip_bounds2_2 = test_tooltip_->anchor_point();

  EXPECT_NE(tooltip_bounds2_1, tooltip_bounds2_2);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  EXPECT_EQ(alternative_string, helper_->GetTooltipText());

  // Test if moving from a view that is contained by a larger view, both with
  // the same tooltip text, does not change tooltip's position.
  center = v1_1->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(v1_1, view_, &center);
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  gfx::Point tooltip_bounds1_1 = test_tooltip_->anchor_point();

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);
  EXPECT_EQ(reference_string, helper_->GetTooltipText());

  center = v1->bounds().CenterPoint();
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  tooltip_bounds1 = test_tooltip_->anchor_point();

  EXPECT_NE(tooltip_bounds1_1, tooltip_bounds1);
  EXPECT_EQ(reference_string, helper_->GetTooltipText());
}

class TooltipStateManagerTest : public TooltipControllerTest {
 public:
  TooltipStateManagerTest() = default;

  TooltipStateManagerTest(const TooltipStateManagerTest&) = delete;
  TooltipStateManagerTest& operator=(const TooltipStateManagerTest&) = delete;

  ~TooltipStateManagerTest() override = default;
};

TEST_F(TooltipStateManagerTest, ShowAndHideTooltip) {
  EXPECT_EQ(nullptr, helper_->state_manager()->tooltip_parent_window());
  EXPECT_EQ(std::u16string(), helper_->state_manager()->tooltip_text());

  std::u16string expected_text = u"Tooltip Text";

  helper_->state_manager()->Show(GetRootWindow(), expected_text,
                                 gfx::Point(0, 0), TooltipTrigger::kCursor,
                                 helper_->GetShowTooltipDelay(), {});

  EXPECT_EQ(GetRootWindow(), helper_->state_manager()->tooltip_parent_window());
  EXPECT_EQ(expected_text, helper_->state_manager()->tooltip_text());
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(helper_->state_manager()->tooltip_trigger(),
            TooltipTrigger::kCursor);

  helper_->HideAndReset();

  EXPECT_EQ(nullptr, helper_->state_manager()->tooltip_parent_window());
  // We don't clear the text of the next tooltip because we use to validate that
  // we're not about to show a tooltip that has been explicitly hidden.
  // TODO(bebeaudr): Update this when we have a truly unique tooltipd id, even
  // for web content.
  EXPECT_EQ(expected_text, helper_->state_manager()->tooltip_text());
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

// Disabled on Lacros since TooltipLacros cannot handle tooltip with delay on
// client side properly. To test with delay, it needs to use Ash server with
// ui_controls in interactive_ui_tests.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ShowTooltipWithDelay DISABLED_ShowTooltipWithDelay
#else
#define MAYBE_ShowTooltipWithDelay ShowTooltipWithDelay
#endif
TEST_F(TooltipStateManagerTest, MAYBE_ShowTooltipWithDelay) {
  EXPECT_EQ(nullptr, helper_->state_manager()->tooltip_parent_window());
  EXPECT_EQ(std::u16string(), helper_->state_manager()->tooltip_text());

  std::u16string expected_text = u"Tooltip Text";

  helper_->SkipTooltipShowDelay(false);

  // 1. Showing the tooltip will start the |will_show_tooltip_timer_| and set
  // the attributes, but won't make the tooltip visible.
  helper_->state_manager()->Show(GetRootWindow(), expected_text,
                                 gfx::Point(0, 0), TooltipTrigger::kCursor,
                                 helper_->GetShowTooltipDelay(), {});
  EXPECT_EQ(GetRootWindow(), helper_->state_manager()->tooltip_parent_window());
  EXPECT_EQ(expected_text, helper_->state_manager()->tooltip_text());
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsWillShowTooltipTimerRunning());

  // 2. Showing the tooltip again with a different expected text will cancel the
  // existing timers running and will update the text, but it still won't make
  // the tooltip visible.
  expected_text = u"Tooltip Text 2";
  helper_->state_manager()->Show(GetRootWindow(), expected_text,
                                 gfx::Point(0, 0), TooltipTrigger::kCursor,
                                 helper_->GetShowTooltipDelay(), {});
  EXPECT_EQ(GetRootWindow(), helper_->state_manager()->tooltip_parent_window());
  EXPECT_EQ(expected_text, helper_->state_manager()->tooltip_text());
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsWillShowTooltipTimerRunning());

  // 3. Calling HideAndReset should cancel the timer running.
  helper_->HideAndReset();
  EXPECT_EQ(nullptr, helper_->state_manager()->tooltip_parent_window());
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_FALSE(helper_->IsWillShowTooltipTimerRunning());

  helper_->SkipTooltipShowDelay(true);
}

// Disabled on Lacros since TooltipLacros cannot handle tooltip with delay on
// client side properly. To test with delay, it needs to use Ash server with
// ui_controls in interactive_ui_tests.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_UpdatePositionIfNeeded DISABLED_UpdatePositionIfNeeded
#else
#define MAYBE_UpdatePositionIfNeeded UpdatePositionIfNeeded
#endif
// This test ensures that we can update the position of the tooltip after the
// |will_show_tooltip_timer_| has been started. This is needed because the
// cursor might still move between the moment Show is called and the timer
// fires.
TEST_F(TooltipStateManagerTest, MAYBE_UpdatePositionIfNeeded) {
  EXPECT_EQ(nullptr, helper_->state_manager()->tooltip_parent_window());
  EXPECT_EQ(std::u16string(), helper_->state_manager()->tooltip_text());

  std::u16string expected_text = u"Tooltip Text";

  helper_->SkipTooltipShowDelay(false);

  {
    gfx::Point position(0, 0);
    // 1. When the |will_show_tooltip_timer_| is running, validate that we can
    // update the position.
    helper_->state_manager()->Show(GetRootWindow(), expected_text, position,
                                   TooltipTrigger::kCursor,
                                   helper_->GetShowTooltipDelay(), {});
    EXPECT_EQ(GetRootWindow(),
              helper_->state_manager()->tooltip_parent_window());
    EXPECT_EQ(expected_text, helper_->state_manager()->tooltip_text());
    EXPECT_EQ(position, helper_->GetTooltipPosition());
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_TRUE(helper_->IsWillShowTooltipTimerRunning());

    gfx::Point new_position = gfx::Point(10, 10);
    // Because the tooltip was triggered by the cursor, the position should be
    // updated by a keyboard triggered modification.
    helper_->state_manager()->UpdatePositionIfNeeded(new_position,
                                                     TooltipTrigger::kKeyboard);
    EXPECT_EQ(position, helper_->GetTooltipPosition());

    // But it should be updated when the position's update is triggered by the
    // cursor.
    helper_->state_manager()->UpdatePositionIfNeeded(new_position,
                                                     TooltipTrigger::kCursor);
    EXPECT_EQ(new_position, helper_->GetTooltipPosition());

    // 2. Validate that we can't update the position when the timer isn't
    // running.
    helper_->HideAndReset();
    position = new_position;
    new_position = gfx::Point(20, 20);
    helper_->state_manager()->UpdatePositionIfNeeded(new_position,
                                                     TooltipTrigger::kCursor);
    EXPECT_EQ(position, helper_->GetTooltipPosition());
  }

  {
    gfx::Point position(0, 0);
    // 1. When the |will_show_tooltip_timer_| is running, validate that we can
    // update the position.
    helper_->state_manager()->Show(GetRootWindow(), expected_text, position,
                                   TooltipTrigger::kKeyboard,
                                   helper_->GetShowTooltipDelay(), {});
    EXPECT_EQ(GetRootWindow(),
              helper_->state_manager()->tooltip_parent_window());
    EXPECT_EQ(expected_text, helper_->state_manager()->tooltip_text());
    EXPECT_EQ(position, helper_->GetTooltipPosition());
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_TRUE(helper_->IsWillShowTooltipTimerRunning());

    gfx::Point new_position = gfx::Point(10, 10);
    // Because the tooltip was triggered by the keyboard, the position shouldn't
    // be updated by a cursor triggered modification.
    helper_->state_manager()->UpdatePositionIfNeeded(new_position,
                                                     TooltipTrigger::kCursor);
    EXPECT_EQ(position, helper_->GetTooltipPosition());

    // But it should be updated when the position's update is triggered by a
    // keyboard action.
    helper_->state_manager()->UpdatePositionIfNeeded(new_position,
                                                     TooltipTrigger::kKeyboard);
    EXPECT_EQ(new_position, helper_->GetTooltipPosition());

    // 2. Validate that we can't update the position when the timer isn't
    // running.
    helper_->HideAndReset();
    position = new_position;
    new_position = gfx::Point(20, 20);
    helper_->state_manager()->UpdatePositionIfNeeded(new_position,
                                                     TooltipTrigger::kKeyboard);
    EXPECT_EQ(position, helper_->GetTooltipPosition());
  }

  helper_->SkipTooltipShowDelay(true);
}

}  // namespace views::corewm::test
