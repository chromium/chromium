// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/event_observer.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/buildflags.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/mock_drag_controller.h"
#include "ui/views/test/mock_native_widget.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_test_api.h"
#include "ui/views/views_test_suite.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_deletion_observer.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"
#include "ui/views/widget/widget_removals_observer.h"
#include "ui/views/widget/widget_utils.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/native_frame_view.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/view_prop.h"
#include "ui/views/test/test_platform_native_widget.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_controller_delegate.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/window_event_target.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

namespace views::test {

namespace {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Not;

// TODO(tdanderson): This utility function is used in different unittest
//                   files. Move to a common location to avoid
//                   repeated code.
gfx::Point ConvertPointFromWidgetToView(View* view, const gfx::Point& p) {
  gfx::Point tmp(p);
  View::ConvertPointToTarget(view->GetWidget()->GetRootView(), view, &tmp);
  return tmp;
}

std::unique_ptr<ui::test::EventGenerator> CreateEventGenerator(
    gfx::NativeWindow root_window,
    gfx::NativeWindow target_window) {
  auto generator =
      std::make_unique<ui::test::EventGenerator>(root_window, target_window);
  return generator;
}

class TestBubbleDialogDelegateView : public BubbleDialogDelegateView {
  METADATA_HEADER(TestBubbleDialogDelegateView, BubbleDialogDelegateView)

 public:
  explicit TestBubbleDialogDelegateView(View* anchor)
      : BubbleDialogDelegateView(anchor, BubbleBorder::NONE) {
    SetOwnedByWidget(false);
  }
  ~TestBubbleDialogDelegateView() override = default;

  bool ShouldShowCloseButton() const override {
    reset_controls_called_ = true;
    return true;
  }

  mutable bool reset_controls_called_ = false;
};

BEGIN_METADATA(TestBubbleDialogDelegateView)
END_METADATA

// Convenience to make constructing a GestureEvent simpler.
ui::GestureEvent CreateTestGestureEvent(ui::EventType type, int x, int y) {
  return ui::GestureEvent(x, y, 0, base::TimeTicks(),
                          ui::GestureEventDetails(type));
}

ui::GestureEvent CreateTestGestureEvent(const ui::GestureEventDetails& details,
                                        int x,
                                        int y) {
  return ui::GestureEvent(x, y, 0, base::TimeTicks(), details);
}

class TestWidgetRemovalsObserver : public WidgetRemovalsObserver {
 public:
  TestWidgetRemovalsObserver() = default;

  TestWidgetRemovalsObserver(const TestWidgetRemovalsObserver&) = delete;
  TestWidgetRemovalsObserver& operator=(const TestWidgetRemovalsObserver&) =
      delete;

  ~TestWidgetRemovalsObserver() override = default;

  void OnWillRemoveView(Widget* widget, View* view) override {
    removed_views_.insert(view);
  }

  bool DidRemoveView(View* view) {
    return removed_views_.find(view) != removed_views_.end();
  }

 private:
  std::set<raw_ptr<View, SetExperimental>> removed_views_;
};

}  // namespace

// A view that keeps track of the events it receives, and consumes all scroll
// gesture events and ui::EventType::kScroll events.
class ScrollableEventCountView : public EventCountView {
  METADATA_HEADER(ScrollableEventCountView, EventCountView)

 public:
  ScrollableEventCountView() = default;

  ScrollableEventCountView(const ScrollableEventCountView&) = delete;
  ScrollableEventCountView& operator=(const ScrollableEventCountView&) = delete;

  ~ScrollableEventCountView() override = default;

 private:
  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    EventCountView::OnGestureEvent(event);
    switch (event->type()) {
      case ui::EventType::kGestureScrollBegin:
      case ui::EventType::kGestureScrollUpdate:
      case ui::EventType::kGestureScrollEnd:
      case ui::EventType::kScrollFlingStart:
        event->SetHandled();
        break;
      default:
        break;
    }
  }

  void OnScrollEvent(ui::ScrollEvent* event) override {
    EventCountView::OnScrollEvent(event);
    if (event->type() == ui::EventType::kScroll) {
      event->SetHandled();
    }
  }
};

BEGIN_METADATA(ScrollableEventCountView)
END_METADATA

// A view that implements GetMinimumSize.
class MinimumSizeFrameView : public NativeFrameView {
  METADATA_HEADER(MinimumSizeFrameView, NativeFrameView)

 public:
  explicit MinimumSizeFrameView(Widget* frame) : NativeFrameView(frame) {}

  MinimumSizeFrameView(const MinimumSizeFrameView&) = delete;
  MinimumSizeFrameView& operator=(const MinimumSizeFrameView&) = delete;

  ~MinimumSizeFrameView() override = default;

 private:
  // Overridden from View:
  gfx::Size GetMinimumSize() const override { return gfx::Size(300, 400); }
};

BEGIN_METADATA(MinimumSizeFrameView)
END_METADATA

// An event handler that simply keeps a count of the different types of events
// it receives.
class EventCountHandler : public ui::EventHandler {
 public:
  EventCountHandler() = default;

  EventCountHandler(const EventCountHandler&) = delete;
  EventCountHandler& operator=(const EventCountHandler&) = delete;

  ~EventCountHandler() override = default;

  int GetEventCount(ui::EventType type) { return event_count_[type]; }

  void ResetCounts() { event_count_.clear(); }

 protected:
  // Overridden from ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    RecordEvent(*event);
    ui::EventHandler::OnEvent(event);
  }

 private:
  void RecordEvent(const ui::Event& event) { ++event_count_[event.type()]; }

  std::map<ui::EventType, int> event_count_;
};

TEST_F(WidgetTest, WidgetInitParams) {
  // Widgets are not transparent by default.
  Widget::InitParams init1(Widget::InitParams::CLIENT_OWNS_WIDGET);
  EXPECT_EQ(Widget::InitParams::WindowOpacity::kInferred, init1.opacity);
}

// Tests that the internal name is propagated through widget initialization to
// the native widget and back.
class WidgetWithCustomParamsTest : public WidgetTest {
 public:
  using InitFunction = base::RepeatingCallback<void(Widget::InitParams*)>;
  void SetInitFunction(const InitFunction& init) { init_ = std::move(init); }
  Widget::InitParams CreateParams(Widget::InitParams::Ownership ownership,
                                  Widget::InitParams::Type type) override {
    Widget::InitParams params = WidgetTest::CreateParams(ownership, type);
    DCHECK(init_) << "If you don't need an init function, use WidgetTest";
    init_.Run(&params);
    return params;
  }

 private:
  InitFunction init_;
};

TEST_F(WidgetWithCustomParamsTest, NamePropagatedFromParams) {
  SetInitFunction(base::BindLambdaForTesting(
      [](Widget::InitParams* params) { params->name = "MyWidget"; }));
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  EXPECT_EQ("MyWidget", widget->native_widget_private()->GetName());
  EXPECT_EQ("MyWidget", widget->GetName());
}

TEST_F(WidgetWithCustomParamsTest, NamePropagatedFromDelegate) {
  WidgetDelegate delegate;
  delegate.set_internal_name("Foobar");
  SetInitFunction(base::BindLambdaForTesting(
      [&](Widget::InitParams* params) { params->delegate = &delegate; }));

  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  EXPECT_EQ(delegate.internal_name(),
            widget->native_widget_private()->GetName());
  EXPECT_EQ(delegate.internal_name(), widget->GetName());
}

// Test that Widget::InitParams::autosize allows widget to
// automatically resize when content view size changes.
TEST_F(WidgetWithCustomParamsTest, Autosize) {
  SetInitFunction(base::BindLambdaForTesting(
      [](Widget::InitParams* params) { params->autosize = true; }));

  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* view = widget->SetContentsView(std::make_unique<views::View>());
  widget->Show();

  constexpr gfx::Size kInitialSize(100, 100);
  view->SetPreferredSize(kInitialSize);
  const gfx::Size starting_size = widget->GetWindowBoundsInScreen().size();

  constexpr gfx::Size kDelta(50, 50);
  view->SetPreferredSize(kInitialSize + kDelta);
  const gfx::Size ending_size = widget->GetWindowBoundsInScreen().size();

  EXPECT_EQ(ending_size, starting_size + kDelta);
}

namespace {

class ViewWithClassName : public View {
  METADATA_HEADER(ViewWithClassName, View)
};

BEGIN_METADATA(ViewWithClassName)
END_METADATA

}  // namespace

TEST_F(WidgetWithCustomParamsTest, NamePropagatedFromContentsViewClassName) {
  WidgetDelegate delegate;
  auto view = std::make_unique<ViewWithClassName>();
  auto* contents = delegate.SetContentsView(std::move(view));
  SetInitFunction(base::BindLambdaForTesting(
      [&](Widget::InitParams* params) { params->delegate = &delegate; }));

  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  EXPECT_EQ(contents->GetClassName(),
            widget->native_widget_private()->GetName());
  EXPECT_EQ(contents->GetClassName(), widget->GetName());
}

namespace {

class TestView : public View {
  METADATA_HEADER(TestView, View)

 public:
  ~TestView() override = default;

  void OnThemeChanged() override {
    View::OnThemeChanged();
    auto* native_theme = GetNativeTheme();
    if (native_theme && native_theme->user_color()) {
      user_color_ = *native_theme->user_color();
    }
  }

  SkColor user_color() const { return user_color_; }

 private:
  SkColor user_color_ = SK_ColorWHITE;
};

BEGIN_METADATA(TestView)
END_METADATA

}  // namespace

TEST_F(WidgetWithCustomParamsTest, InitWithNativeTheme) {
  // Verify that `InitParams::native_theme` is applied during widget
  // initialization.

  const SkColor test_color = SkColorSetARGB(1, 2, 3, 4);

  WidgetDelegate delegate;
  auto view = std::make_unique<TestView>();
  auto* view_raw_ptr = view.get();
  delegate.SetContentsView(std::move(view));

  ui::TestNativeTheme test_native_theme;
  test_native_theme.set_user_color(test_color);

  SetInitFunction(base::BindLambdaForTesting([&](Widget::InitParams* params) {
    params->delegate = &delegate;
    params->native_theme = &test_native_theme;
  }));

  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  EXPECT_EQ(view_raw_ptr->user_color(), test_color);
}

class WidgetColorModeTest : public WidgetTest {
 public:
  static constexpr SkColor kLightColor = SK_ColorWHITE;
  static constexpr SkColor kDarkColor = SK_ColorBLACK;

  WidgetColorModeTest() = default;
  ~WidgetColorModeTest() override = default;

  void SetUp() override {
    WidgetTest::SetUp();

    // Setup color provider for the ui::kColorSysPrimary color.
    ui::ColorProviderManager& manager =
        ui::ColorProviderManager::GetForTesting();
    manager.AppendColorProviderInitializer(base::BindRepeating(&AddColor));
  }

  void TearDown() override {
    ui::ColorProviderManager::ResetForTesting();
    WidgetTest::TearDown();
  }

 private:
  static void AddColor(ui::ColorProvider* provider,
                       const ui::ColorProviderKey& key) {
    ui::ColorMixer& mixer = provider->AddMixer();
    mixer[ui::kColorSysPrimary] = {
        key.color_mode == ui::ColorProviderKey::ColorMode::kDark ? kDarkColor
                                                                 : kLightColor};
  }
};

TEST_F(WidgetColorModeTest, ColorModeOverride_NoOverride) {
  ui::TestNativeTheme test_theme;
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  test_theme.SetDarkMode(true);
  widget->SetNativeThemeForTest(&test_theme);

  widget->SetColorModeOverride({});
  // Verify that we resolve the dark color when we don't override color mode.
  EXPECT_EQ(kDarkColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
}

TEST_F(WidgetColorModeTest, ColorModeOverride_DarkOverride) {
  ui::TestNativeTheme test_theme;
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  test_theme.SetDarkMode(false);
  widget->SetNativeThemeForTest(&test_theme);

  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);
  // Verify that we resolve the light color even though the theme is dark.
  EXPECT_EQ(kDarkColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
}

TEST_F(WidgetColorModeTest, ColorModeOverride_LightOverride) {
  ui::TestNativeTheme test_theme;
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  test_theme.SetDarkMode(true);
  widget->SetNativeThemeForTest(&test_theme);

  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kLight);
  // Verify that we resolve the light color even though the theme is dark.
  EXPECT_EQ(kLightColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
}

TEST_F(WidgetColorModeTest, ChildInheritsColorMode_NoOverrides) {
  // Create the parent widget and set the native theme to dark.
  ui::TestNativeTheme test_theme;
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  test_theme.SetDarkMode(true);
  widget->SetNativeThemeForTest(&test_theme);

  // Create the child widget.
  std::unique_ptr<Widget> widget_child =
      base::WrapUnique(CreateChildPlatformWidget(
          widget->GetNativeView(), Widget::InitParams::CLIENT_OWNS_WIDGET));

  // Ensure neither has an override set. The child should inherit the color mode
  // of the parent.
  widget->SetColorModeOverride({});
  widget_child->SetColorModeOverride({});
  EXPECT_EQ(kDarkColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
  EXPECT_EQ(kDarkColor,
            widget_child->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  // Set the parent's native theme to light. The child should inherit the color
  // mode of the parent.
  test_theme.SetDarkMode(false);
  EXPECT_EQ(kLightColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
  EXPECT_EQ(kLightColor,
            widget_child->GetColorProvider()->GetColor(ui::kColorSysPrimary));
}

TEST_F(WidgetColorModeTest, ChildInheritsColorMode_Overrides) {
  // Create the parent widget and set the native theme to dark.
  ui::TestNativeTheme test_theme;
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  test_theme.SetDarkMode(true);
  widget->SetNativeThemeForTest(&test_theme);

  // Create the child widget.
  std::unique_ptr<Widget> widget_child =
      base::WrapUnique(CreateChildPlatformWidget(
          widget->GetNativeView(), Widget::InitParams::CLIENT_OWNS_WIDGET));

  // Ensure neither has an override set. The child should inherit the color mode
  // of the parent.
  widget->SetColorModeOverride({});
  widget_child->SetColorModeOverride({});
  EXPECT_EQ(kDarkColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
  EXPECT_EQ(kDarkColor,
            widget_child->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  // Set the parent's override to light, then back to dark. the child should
  // follow the parent's overridden color mode.
  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kLight);
  EXPECT_EQ(kLightColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
  EXPECT_EQ(kLightColor,
            widget_child->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);
  EXPECT_EQ(kDarkColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
  EXPECT_EQ(kDarkColor,
            widget_child->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  // Override the child's color mode to light. The parent should continue to
  // report a dark color mode.
  widget_child->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kLight);
  EXPECT_EQ(kDarkColor,
            widget->GetColorProvider()->GetColor(ui::kColorSysPrimary));
  EXPECT_EQ(kLightColor,
            widget_child->GetColorProvider()->GetColor(ui::kColorSysPrimary));
}

TEST_F(WidgetTest, NativeWindowProperty) {
  const char* key = "foo";
  int value = 3;

  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  EXPECT_EQ(nullptr, widget->GetNativeWindowProperty(key));

  widget->SetNativeWindowProperty(key, &value);
  EXPECT_EQ(&value, widget->GetNativeWindowProperty(key));

  widget->SetNativeWindowProperty(key, nullptr);
  EXPECT_EQ(nullptr, widget->GetNativeWindowProperty(key));
}

TEST_F(WidgetTest, GetParent) {
  // Create a hierarchy of native widgets.
  std::unique_ptr<Widget> toplevel = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> child = base::WrapUnique(CreateChildPlatformWidget(
      toplevel->GetNativeView(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> grandchild =
      base::WrapUnique(CreateChildPlatformWidget(
          child->GetNativeView(), Widget::InitParams::CLIENT_OWNS_WIDGET));

  EXPECT_EQ(nullptr, toplevel->parent());
  EXPECT_EQ(child.get(), grandchild->parent());
  EXPECT_EQ(toplevel.get(), child->parent());

  // children should be automatically destroyed with |toplevel|.
}

// Verify that there is no change in focus if |enable_arrow_key_traversal| is
// false (the default).
TEST_F(WidgetTest, ArrowKeyFocusTraversalOffByDefault) {
  std::unique_ptr<Widget> toplevel = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));

  // Establish default value.
  DCHECK(!toplevel->widget_delegate()->enable_arrow_key_traversal());

  View* container = toplevel->client_view();
  container->SetLayoutManager(std::make_unique<FillLayout>());
  auto* const button1 =
      container->AddChildView(std::make_unique<LabelButton>());
  auto* const button2 =
      container->AddChildView(std::make_unique<LabelButton>());
  toplevel->Show();
  button1->RequestFocus();

  ui::KeyEvent right_arrow(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                           ui::EF_NONE);
  toplevel->OnKeyEvent(&right_arrow);
  EXPECT_TRUE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());

  ui::KeyEvent left_arrow(ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                          ui::EF_NONE);
  toplevel->OnKeyEvent(&left_arrow);
  EXPECT_TRUE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());

  ui::KeyEvent up_arrow(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE);
  toplevel->OnKeyEvent(&up_arrow);
  EXPECT_TRUE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());

  ui::KeyEvent down_arrow(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                          ui::EF_NONE);
  toplevel->OnKeyEvent(&down_arrow);
  EXPECT_TRUE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());
}

// Verify that arrow keys can change focus if |enable_arrow_key_traversal| is
// set to true.
TEST_F(WidgetTest, ArrowKeyTraversalMovesFocusBetweenViews) {
  std::unique_ptr<Widget> toplevel = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  toplevel->widget_delegate()->SetEnableArrowKeyTraversal(true);

  View* container = toplevel->client_view();
  container->SetLayoutManager(std::make_unique<FillLayout>());
  auto* const button1 =
      container->AddChildView(std::make_unique<LabelButton>());
  auto* const button2 =
      container->AddChildView(std::make_unique<LabelButton>());
  auto* const button3 =
      container->AddChildView(std::make_unique<LabelButton>());
  toplevel->Show();
  button1->RequestFocus();

  // Right should advance focus (similar to TAB).
  ui::KeyEvent right_arrow(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                           ui::EF_NONE);
  toplevel->OnKeyEvent(&right_arrow);
  EXPECT_FALSE(button1->HasFocus());
  EXPECT_TRUE(button2->HasFocus());
  EXPECT_FALSE(button3->HasFocus());

  // Down should also advance focus.
  ui::KeyEvent down_arrow(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                          ui::EF_NONE);
  toplevel->OnKeyEvent(&down_arrow);
  EXPECT_FALSE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());
  EXPECT_TRUE(button3->HasFocus());

  // Left should reverse focus (similar to SHIFT+TAB).
  ui::KeyEvent left_arrow(ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                          ui::EF_NONE);
  toplevel->OnKeyEvent(&left_arrow);
  EXPECT_FALSE(button1->HasFocus());
  EXPECT_TRUE(button2->HasFocus());
  EXPECT_FALSE(button3->HasFocus());

  // Up should also reverse focus.
  ui::KeyEvent up_arrow(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE);
  toplevel->OnKeyEvent(&up_arrow);
  EXPECT_TRUE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());
  EXPECT_FALSE(button3->HasFocus());

  // Test backwards wrap-around.
  ui::KeyEvent up_arrow2(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE);
  toplevel->OnKeyEvent(&up_arrow2);
  EXPECT_FALSE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());
  EXPECT_TRUE(button3->HasFocus());

  // Test forward wrap-around.
  ui::KeyEvent down_arrow2(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                           ui::EF_NONE);
  toplevel->OnKeyEvent(&down_arrow2);
  EXPECT_TRUE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());
  EXPECT_FALSE(button3->HasFocus());
}

TEST_F(WidgetTest, ArrowKeyTraversalNotInheritedByChildWidgets) {
  std::unique_ptr<Widget> parent = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> child = base::WrapUnique(CreateChildPlatformWidget(
      parent->GetNativeView(), Widget::InitParams::CLIENT_OWNS_WIDGET));

  parent->widget_delegate()->SetEnableArrowKeyTraversal(true);

  View* container = child->GetContentsView();
  DCHECK(container);
  container->SetLayoutManager(std::make_unique<FillLayout>());
  auto* const button1 =
      container->AddChildView(std::make_unique<LabelButton>());
  auto* const button2 =
      container->AddChildView(std::make_unique<LabelButton>());
  parent->Show();
  child->Show();
  button1->RequestFocus();

  // Arrow key should not cause focus change on child since only the parent
  // Widget has |enable_arrow_key_traversal| set.
  ui::KeyEvent right_arrow(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                           ui::EF_NONE);
  child->OnKeyEvent(&right_arrow);
  EXPECT_TRUE(button1->HasFocus());
  EXPECT_FALSE(button2->HasFocus());
}

TEST_F(WidgetTest, ArrowKeyTraversalMayBeExplicitlyEnabledByChildWidgets) {
  std::unique_ptr<Widget> parent = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> child = base::WrapUnique(CreateChildPlatformWidget(
      parent->GetNativeView(), Widget::InitParams::CLIENT_OWNS_WIDGET));

  child->widget_delegate()->SetEnableArrowKeyTraversal(true);

  View* container = child->GetContentsView();
  container->SetLayoutManager(std::make_unique<FillLayout>());
  auto* const button1 =
      container->AddChildView(std::make_unique<LabelButton>());
  auto* const button2 =
      container->AddChildView(std::make_unique<LabelButton>());
  parent->Show();
  child->Show();
  button1->RequestFocus();

  // Arrow key should cause focus key on child since child has flag set, even
  // if the parent Widget does not.
  ui::KeyEvent right_arrow(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                           ui::EF_NONE);
  child->OnKeyEvent(&right_arrow);
  EXPECT_FALSE(button1->HasFocus());
  EXPECT_TRUE(button2->HasFocus());
}

////////////////////////////////////////////////////////////////////////////////
// Widget::GetTopLevelWidget tests.

TEST_F(WidgetTest, GetTopLevelWidget_Native) {
  // Create a hierarchy of native widgets.
  std::unique_ptr<Widget> toplevel = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  gfx::NativeView parent = toplevel->GetNativeView();
  std::unique_ptr<Widget> child = base::WrapUnique(CreateChildPlatformWidget(
      parent, Widget::InitParams::CLIENT_OWNS_WIDGET));

  EXPECT_EQ(toplevel.get(), toplevel->GetTopLevelWidget());
  EXPECT_EQ(toplevel.get(), child->GetTopLevelWidget());

  // |child| should be automatically destroyed with |toplevel|.
}

// Test if a focus manager and an inputmethod work without CHECK failure
// when window activation changes.
TEST_F(WidgetTest, ChangeActivation) {
  std::unique_ptr<Widget> top1 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  top1->Show();
  RunPendingMessages();

  std::unique_ptr<Widget> top2 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  top2->Show();
  RunPendingMessages();

  top1->Activate();
  RunPendingMessages();

  top2->Activate();
  RunPendingMessages();

  top1->Activate();
  RunPendingMessages();
}

// Tests visibility of child widgets.
TEST_F(WidgetTest, Visibility) {
  std::unique_ptr<Widget> toplevel = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  gfx::NativeView parent = toplevel->GetNativeView();
  std::unique_ptr<Widget> child = base::WrapUnique(CreateChildPlatformWidget(
      parent, Widget::InitParams::CLIENT_OWNS_WIDGET));

  EXPECT_FALSE(toplevel->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  // Showing a child with a hidden parent keeps the child hidden.
  child->Show();
  EXPECT_FALSE(toplevel->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  // Showing a hidden parent with a visible child shows both.
  toplevel->Show();
  EXPECT_TRUE(toplevel->IsVisible());
  EXPECT_TRUE(child->IsVisible());

  // Hiding a parent hides both parent and child.
  toplevel->Hide();
  EXPECT_FALSE(toplevel->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  // Hiding a child while the parent is hidden keeps the child hidden when the
  // parent is shown.
  child->Hide();
  toplevel->Show();
  EXPECT_TRUE(toplevel->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  // |child| should be automatically destroyed with |toplevel|.
}

// Test that child widgets are positioned relative to their parent.
TEST_F(WidgetTest, ChildBoundsRelativeToParent) {
  std::unique_ptr<Widget> toplevel = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> child = base::WrapUnique(CreateChildPlatformWidget(
      toplevel->GetNativeView(), Widget::InitParams::CLIENT_OWNS_WIDGET));

  toplevel->SetBounds(gfx::Rect(160, 100, 320, 200));
  child->SetBounds(gfx::Rect(0, 0, 320, 200));

  child->Show();
  toplevel->Show();

  gfx::Rect toplevel_bounds = toplevel->GetWindowBoundsInScreen();

  // Check the parent origin. If it was (0, 0) the test wouldn't be interesting.
  EXPECT_NE(gfx::Vector2d(0, 0), toplevel_bounds.OffsetFromOrigin());

  // The child's origin is at (0, 0), but the same size, so bounds should match.
  EXPECT_EQ(toplevel_bounds, child->GetWindowBoundsInScreen());
}

////////////////////////////////////////////////////////////////////////////////
// Widget ownership tests.
//
// Tests various permutations of Widget ownership specified in the
// InitParams::Ownership param. Make sure that they are properly destructed
// during shutdown.

// A bag of state to monitor destructions.
struct OwnershipTestState {
  OwnershipTestState() = default;

  bool widget_deleted = false;
  bool native_widget_deleted = false;
};

class WidgetOwnershipTest : public WidgetTest {
 public:
  WidgetOwnershipTest() = default;

  WidgetOwnershipTest(const WidgetOwnershipTest&) = delete;
  WidgetOwnershipTest& operator=(const WidgetOwnershipTest&) = delete;

  ~WidgetOwnershipTest() override = default;

  void TearDown() override {
    EXPECT_TRUE(state()->widget_deleted);
    EXPECT_TRUE(state()->native_widget_deleted);
    WidgetTest::TearDown();
  }

  OwnershipTestState* state() { return &state_; }

 private:
  OwnershipTestState state_;
};

// A Widget subclass that updates a bag of state when it is destroyed.
class OwnershipTestWidget : public Widget {
 public:
  explicit OwnershipTestWidget(OwnershipTestState* state) : state_(state) {}

  OwnershipTestWidget(const OwnershipTestWidget&) = delete;
  OwnershipTestWidget& operator=(const OwnershipTestWidget&) = delete;

  ~OwnershipTestWidget() override { state_->widget_deleted = true; }

 private:
  raw_ptr<OwnershipTestState> state_;
};

class NativeWidgetDestroyedWaiter {
 public:
  explicit NativeWidgetDestroyedWaiter(OwnershipTestState* state)
      : state_(state) {}

  base::OnceClosure GetNativeWidgetDestroyedCallback() {
    return base::BindOnce(
        [](OwnershipTestState* state, base::RunLoop* run_loop) {
          state->native_widget_deleted = true;
          run_loop->Quit();
        },
        state_.get(), &run_loop_);
  }

  void Wait() {
    if (!state_->native_widget_deleted) {
      run_loop_.Run();
    }
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<OwnershipTestState> state_;
};

using NativeWidgetOwnsWidgetTest = WidgetOwnershipTest;
// NativeWidget owns its Widget, part 1.1: NativeWidget is a non-desktop
// widget, CloseNow() destroys Widget and NativeWidget synchronously.
TEST_F(NativeWidgetOwnsWidgetTest, NonDesktopWidget_CloseNow) {
  Widget* widget = new OwnershipTestWidget(state());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget, kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));

  widget->CloseNow();

  // Both widget and native widget should be deleted synchronously.
  EXPECT_TRUE(state()->widget_deleted);
  EXPECT_TRUE(state()->native_widget_deleted);
}

// NativeWidget owns its Widget, part 1.2: NativeWidget is a non-desktop
// widget, Close() destroys Widget and NativeWidget asynchronously.
TEST_F(NativeWidgetOwnsWidgetTest, NonDesktopWidget_Close) {
  NativeWidgetDestroyedWaiter waiter(state());
  Widget* widget = new OwnershipTestWidget(state());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget, kStubCapture, waiter.GetNativeWidgetDestroyedCallback());
  widget->Init(std::move(params));

  widget->Close();
  waiter.Wait();

  EXPECT_TRUE(state()->widget_deleted);
  EXPECT_TRUE(state()->native_widget_deleted);
}

// NativeWidget owns its Widget, part 1.3: NativeWidget is a desktop
// widget, Close() destroys Widget and NativeWidget asynchronously.
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
TEST_F(NativeWidgetOwnsWidgetTest, DesktopWidget_Close) {
  NativeWidgetDestroyedWaiter waiter(state());
  Widget* widget = new OwnershipTestWidget(state());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  params.native_widget = CreatePlatformDesktopNativeWidgetImpl(
      widget, kStubCapture, waiter.GetNativeWidgetDestroyedCallback());
  widget->Init(std::move(params));

  widget->Close();
  waiter.Wait();

  EXPECT_TRUE(state()->widget_deleted);
  EXPECT_TRUE(state()->native_widget_deleted);
}
#endif

// NativeWidget owns its Widget, part 1.4: NativeWidget is a desktop
// widget. Unlike desktop widget, CloseNow() might destroy Widget and
// NativeWidget asynchronously.
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
TEST_F(NativeWidgetOwnsWidgetTest, DesktopWidget_CloseNow) {
  NativeWidgetDestroyedWaiter waiter(state());
  Widget* widget = new OwnershipTestWidget(state());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  params.native_widget = CreatePlatformDesktopNativeWidgetImpl(
      widget, kStubCapture, waiter.GetNativeWidgetDestroyedCallback());
  widget->Init(std::move(params));

  widget->CloseNow();
  waiter.Wait();

  EXPECT_TRUE(state()->widget_deleted);
  EXPECT_TRUE(state()->native_widget_deleted);
}
#endif

// NativeWidget owns its Widget, part 2.1: NativeWidget is a non-desktop
// widget. CloseNow() the parent should destroy the child.
TEST_F(NativeWidgetOwnsWidgetTest, NonDestkopWidget_CloseNowParent) {
  NativeWidgetDestroyedWaiter waiter(state());
  Widget* toplevel = CreateTopLevelPlatformWidget();
  Widget* widget = new OwnershipTestWidget(state());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  params.parent = toplevel->GetNativeView();
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget, kStubCapture, waiter.GetNativeWidgetDestroyedCallback());
  widget->Init(std::move(params));

  // Now destroy the native widget. This is achieved by closing the toplevel.
  toplevel->CloseNow();
  // The NativeWidget won't be deleted until after a return to the message loop
  // so we have to run pending messages before testing the destruction status.
  waiter.Wait();

  EXPECT_TRUE(state()->widget_deleted);
  EXPECT_TRUE(state()->native_widget_deleted);
}

// NativeWidget owns its Widget, part 2.2: NativeWidget is a desktop
// widget. CloseNow() the parent should destroy the child.
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
TEST_F(NativeWidgetOwnsWidgetTest, DestkopWidget_CloseNowParent) {
  NativeWidgetDestroyedWaiter waiter(state());
  Widget* toplevel = CreateTopLevelPlatformDesktopWidget();
  Widget* widget = new OwnershipTestWidget(state());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  params.parent = toplevel->GetNativeView();
  params.native_widget = CreatePlatformDesktopNativeWidgetImpl(
      widget, kStubCapture, waiter.GetNativeWidgetDestroyedCallback());
  widget->Init(std::move(params));

  // Now destroy the native widget. This is achieved by closing the toplevel.
  toplevel->CloseNow();
  // The NativeWidget won't be deleted until after a return to the message loop
  // so we have to run pending messages before testing the destruction status.
  waiter.Wait();

  EXPECT_TRUE(state()->widget_deleted);
  EXPECT_TRUE(state()->native_widget_deleted);
}
#endif

// NativeWidget owns its Widget, part 3.1: NativeWidget is a non-desktop
// widget, destroyed out from under it by the OS.
TEST_F(NativeWidgetOwnsWidgetTest, NonDesktopWidget_NativeDestroy) {
  Widget* widget = new OwnershipTestWidget(state());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget, kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));

  // Now simulate a destroy of the platform native widget from the OS:
  SimulateNativeDestroy(widget);

  EXPECT_TRUE(state()->widget_deleted);
  EXPECT_TRUE(state()->native_widget_deleted);
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
// NativeWidget owns its Widget, part 3.2: NativeWidget is a desktop
// widget, destroyed out from under it by the OS.
TEST_F(NativeWidgetOwnsWidgetTest, DesktopWidget_NativeDestroy) {
  NativeWidgetDestroyedWaiter waiter(state());
  Widget* widget = new OwnershipTestWidget(state());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  params.native_widget = CreatePlatformDesktopNativeWidgetImpl(
      widget, kStubCapture, waiter.GetNativeWidgetDestroyedCallback());
  widget->Init(std::move(params));

  // Now simulate a destroy of the platform native widget from the OS:
  SimulateDesktopNativeDestroy(widget);
  waiter.Wait();

  EXPECT_TRUE(state()->widget_deleted);
  EXPECT_TRUE(state()->native_widget_deleted);
}
#endif

using WidgetOwnsNativeWidgetTest = WidgetOwnershipTest;
// Widget owns its NativeWidget, part 1.
TEST_F(WidgetOwnsNativeWidgetTest, Ownership) {
  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));

  // Now delete the Widget, which should delete the NativeWidget.
  widget.reset();

  // TODO(beng): write test for this ownership scenario and the NativeWidget
  //             being deleted out from under the Widget.
}

// Widget owns its NativeWidget, part 2: destroy the parent view.
TEST_F(WidgetOwnsNativeWidgetTest, DestroyParentView) {
  Widget* toplevel = CreateTopLevelPlatformWidget();

  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = toplevel->GetNativeView();
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));

  // Now close the toplevel, which deletes the view hierarchy.
  toplevel->CloseNow();

  RunPendingMessages();

  // This shouldn't delete the widget because it shouldn't be deleted
  // from the native side.
  EXPECT_FALSE(state()->widget_deleted);
  EXPECT_FALSE(state()->native_widget_deleted);
}

// Widget owns its NativeWidget, part 3: has a WidgetDelegateView as contents.
TEST_F(WidgetOwnsNativeWidgetTest, WidgetDelegateView) {
  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  params.delegate = new WidgetDelegateView();
  widget->Init(std::move(params));

  // Allow the Widget to go out of scope. There should be no crash or
  // use-after-free.
}

// Widget owns its NativeWidget, part 4: Widget::CloseNow should be idempotent.
TEST_F(WidgetOwnsNativeWidgetTest, IdempotentCloseNow) {
  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));

  // Now close the Widget, which should delete the NativeWidget.
  widget->CloseNow();

  RunPendingMessages();

  // Close the widget again should not crash.
  widget->CloseNow();

  RunPendingMessages();
}

// Widget owns its NativeWidget, part 5: Widget::Close should be idempotent.
TEST_F(WidgetOwnsNativeWidgetTest, IdempotentClose) {
  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));

  // Now close the Widget, which should delete the NativeWidget.
  widget->Close();

  RunPendingMessages();

  // Close the widget again should not crash.
  widget->Close();

  RunPendingMessages();
}

// Tests for CLIENT_OWNS_WIDGET. The client holds a unique_ptr<Widget>.
// The NativeWidget will be destroyed when the platform window is closed.
using ClientOwnsWidgetTest = WidgetOwnershipTest;

TEST_F(ClientOwnsWidgetTest, Ownership) {
  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));

  widget->CloseNow();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(state()->native_widget_deleted);
}

TEST_F(ClientOwnsWidgetTest, DestructWithAsyncCloseFirst) {
  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));
  widget->Close();
  widget.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(state()->native_widget_deleted);
}

TEST_F(ClientOwnsWidgetTest, DestructWithoutExplicitClose) {
  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));
  widget.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(state()->native_widget_deleted);
}

class WidgetDestroyCounter : public WidgetObserver {
 public:
  explicit WidgetDestroyCounter(Widget* widget)
      : widget_(widget->GetWeakPtr()) {
    widget_->AddObserver(this);
  }
  ~WidgetDestroyCounter() override {
    if (widget_) {
      widget_->RemoveObserver(this);
    }
  }

  int widget_destroying_count() const { return widget_destroying_count_; }

  int widget_destroyed_count() const { return widget_destroyed_count_; }

 private:
  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override {
    ++widget_destroying_count_;
  }

  void OnWidgetDestroyed(Widget* widget) override { ++widget_destroyed_count_; }

  base::WeakPtr<Widget> widget_;
  int widget_destroying_count_ = 0;
  int widget_destroyed_count_ = 0;
};

TEST_F(ClientOwnsWidgetTest, NotificationsTest) {
  auto widget = std::make_unique<OwnershipTestWidget>(state());
  Widget::InitParams params =
      CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = CreatePlatformNativeWidgetImpl(
      widget.get(), kStubCapture, &state()->native_widget_deleted);
  widget->Init(std::move(params));
  auto observer = std::make_unique<WidgetDestroyCounter>(widget.get());
  widget->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer->widget_destroying_count(), 1);
  EXPECT_EQ(observer->widget_destroyed_count(), 1);
  widget.reset();
  EXPECT_TRUE(state()->widget_deleted);
  // The destroying & destroyed notifications should only happen once.
  EXPECT_EQ(observer->widget_destroying_count(), 1);
  EXPECT_EQ(observer->widget_destroyed_count(), 1);
}

////////////////////////////////////////////////////////////////////////////////
// Test to verify using various Widget methods doesn't crash when the underlying
// NativeView and NativeWidget is destroyed. Currently, for
// the WIDGET_OWNS_NATIVE_WIDGET ownership pattern, the NativeWidget will not be
// destroyed, but |native_widget_| will still be set to nullptr.

class WidgetWithDestroyedNativeViewOrNativeWidgetTest
    : public ViewsTestBase,
      public testing::WithParamInterface<
          std::tuple<ViewsTestBase::NativeWidgetType,
                     Widget::InitParams::Ownership>> {
 public:
  WidgetWithDestroyedNativeViewOrNativeWidgetTest() = default;

  WidgetWithDestroyedNativeViewOrNativeWidgetTest(
      const WidgetWithDestroyedNativeViewOrNativeWidgetTest&) = delete;
  WidgetWithDestroyedNativeViewOrNativeWidgetTest& operator=(
      const WidgetWithDestroyedNativeViewOrNativeWidgetTest&) = delete;

  ~WidgetWithDestroyedNativeViewOrNativeWidgetTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    set_native_widget_type(
        std::get<ViewsTestBase::NativeWidgetType>(GetParam()));
    ViewsTestBase::SetUp();
    if (std::get<Widget::InitParams::Ownership>(GetParam()) ==
        Widget::InitParams::CLIENT_OWNS_WIDGET) {
      widget_ = std::make_unique<Widget>();
      Widget::InitParams params =
          CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW_FRAMELESS);
      widget_->Init(std::move(params));
    } else {
      widget_ = CreateTestWidget(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    }
    widget()->Show();
    widget()->native_widget_private()->CloseNow();
    task_environment()->RunUntilIdle();
  }

  Widget* widget() { return widget_.get(); }

  static std::string PrintTestName(
      const ::testing::TestParamInfo<
          WidgetWithDestroyedNativeViewOrNativeWidgetTest::ParamType>& info) {
    std::string test_name;
    switch (std::get<ViewsTestBase::NativeWidgetType>(info.param)) {
      case ViewsTestBase::NativeWidgetType::kDefault:
        test_name += "DefaultNativeWidget";
        break;
      case ViewsTestBase::NativeWidgetType::kDesktop:
        test_name += "DesktopNativeWidget";
        break;
    }
    test_name += "_";
    switch (std::get<Widget::InitParams::Ownership>(info.param)) {
      case Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET:
        test_name += "WidgetOwnsNativeWidget";
        break;
      case Widget::InitParams::CLIENT_OWNS_WIDGET:
        test_name += "ClientOwnsWidget";
        break;
      case Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET:
        // Note: We don't test for this case in
        // WidgetWithDestroyedNativeViewOrNativeWidgetTest.
        test_name += "NativeWidgetOwnsWidget";
        break;
    }
    return test_name;
  }

 private:
  std::unique_ptr<Widget> widget_;
};

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Activate) {
  widget()->Activate();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, AddAndRemoveObserver) {
  // Constructor calls |AddObserver()|
  TestWidgetObserver observer(widget());
  widget()->RemoveObserver(&observer);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       AddAndRemoveRemovalsObserver) {
  TestWidgetRemovalsObserver removals_observer;
  widget()->AddRemovalsObserver(&removals_observer);
  widget()->RemoveRemovalsObserver(&removals_observer);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, AsWidget) {
  widget()->AsWidget();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, CanActivate) {
  widget()->CanActivate();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, CenterWindow) {
  widget()->CenterWindow(gfx::Size());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ClearNativeFocus) {
  widget()->ClearNativeFocus();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ClientView) {
  widget()->client_view();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Close) {
  widget()->Close();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       CloseAllSecondaryWidgets) {
  widget()->CloseAllSecondaryWidgets();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, CloseNow) {
  widget()->CloseNow();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ClosedReason) {
  widget()->closed_reason();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, CloseWithReason) {
  widget()->CloseWithReason(Widget::ClosedReason::kUnspecified);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       CreateNonClientFrameView) {
  widget()->CreateNonClientFrameView();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Deactivate) {
  widget()->Deactivate();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, DraggedView) {
  widget()->dragged_view();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, EndMoveLoop) {
  widget()->EndMoveLoop();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ExecuteCommand) {
  widget()->ExecuteCommand(0);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, FlashFrame) {
  widget()->FlashFrame(true);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, FrameType) {
  widget()->frame_type();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, FrameTypeChanged) {
  widget()->FrameTypeChanged();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetAccelerator) {
  ui::Accelerator accelerator;
  widget()->GetAccelerator(0, &accelerator);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetAllChildWidgets) {
  Widget::Widgets widgets;
  Widget::GetAllChildWidgets(widget()->GetNativeView(), &widgets);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetAllOwnedWidgets) {
  Widget::Widgets widgets;
  Widget::GetAllOwnedWidgets(widget()->GetNativeView(), &widgets);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetAndSetZOrderLevel) {
  widget()->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  widget()->GetZOrderLevel();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       GetClientAreaBoundsInScreen) {
  widget()->GetClientAreaBoundsInScreen();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetColorProvider) {
  widget()->GetColorProvider();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetCompositor) {
  widget()->GetCompositor();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetContentsView) {
  widget()->GetContentsView();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetCustomTheme) {
  widget()->GetCustomTheme();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetEventSink) {
  widget()->GetEventSink();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetFocusSearch) {
  widget()->GetFocusSearch();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetFocusManager) {
  widget()->GetFocusManager();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetFocusTraversable) {
  widget()->GetFocusTraversable();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetGestureConsumer) {
  widget()->GetGestureConsumer();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetGestureRecognizer) {
  widget()->GetGestureRecognizer();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetHitTestMask) {
  SkPath mask;
  widget()->GetHitTestMask(&mask);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetInputMethod) {
  widget()->GetInputMethod();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetLayer) {
  widget()->GetLayer();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetMinimumSize) {
  widget()->GetMinimumSize();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetMaximumSize) {
  widget()->GetMaximumSize();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetName) {
  widget()->GetName();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetNativeTheme) {
  widget()->GetNativeTheme();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetNativeView) {
  widget()->GetNativeView();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetNativeWindow) {
  widget()->GetNativeWindow();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       GetNativeWindowProperty) {
  widget()->GetNativeWindowProperty("xx");
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetNonClientComponent) {
  gfx::Point point;
  widget()->GetNonClientComponent(point);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       GetPrimaryWindowWidget) {
  widget()->GetPrimaryWindowWidget();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetRestoredBounds) {
  widget()->GetRestoredBounds();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetRootView) {
  widget()->GetRootView();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetSublevelManager) {
  widget()->GetSublevelManager();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetThemeProvider) {
  widget()->GetThemeProvider();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetTooltipManager) {
  widget()->GetTooltipManager();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetTopLevelWidget) {
  widget()->GetTopLevelWidget();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       GetTopLevelWidgetForNativeView) {
  Widget::GetTopLevelWidgetForNativeView(widget()->GetNativeView());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetWeakPtr) {
  widget()->GetWeakPtr();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       GetWidgetForNativeView) {
  Widget::GetWidgetForNativeView(widget()->GetNativeView());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       GetWidgetForNativeWindow) {
  Widget::GetWidgetForNativeWindow(widget()->GetNativeWindow());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       GetWindowBoundsInScreen) {
  widget()->GetWindowBoundsInScreen();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       GetWorkAreaBoundsInScreen) {
  widget()->GetWorkAreaBoundsInScreen();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetWorkspace) {
  widget()->GetWorkspace();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, GetZOrderSublevel) {
  widget()->GetZOrderSublevel();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, HasCapture) {
  widget()->HasCapture();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, HasFocusManager) {
  widget()->HasFocusManager();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, HasHitTestMask) {
  widget()->HasHitTestMask();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, HasObserver) {
  TestWidgetObserver observer(widget());
  widget()->HasObserver(&observer);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, HasRemovalsObserver) {
  TestWidgetRemovalsObserver observer;
  widget()->HasRemovalsObserver(&observer);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Hide) {
  widget()->Hide();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Init) {
  Widget::InitParams params(
      std::get<Widget::InitParams::Ownership>(GetParam()));
  EXPECT_DCHECK_DEATH(widget()->Init(std::move(params)));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, is_secondary_widget) {
  widget()->is_secondary_widget();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsActive) {
  widget()->IsActive();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsClosed) {
  widget()->IsClosed();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsDialogBox) {
  widget()->IsDialogBox();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsFullscreen) {
  widget()->IsFullscreen();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsMaximized) {
  widget()->IsMaximized();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsMinimized) {
  widget()->IsMinimized();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsModal) {
  widget()->IsModal();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsMouseEventsEnabled) {
  widget()->IsMouseEventsEnabled();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsMoveLoopSupported) {
  widget()->IsMoveLoopSupported();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       IsNativeWidgetInitialized) {
  widget()->IsNativeWidgetInitialized();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsStackedAbove) {
  std::unique_ptr<Widget> other_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget()->IsStackedAbove(other_widget->GetNativeView());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, IsVisible) {
  widget()->IsVisible();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       IsVisibleOnAllWorkspaces) {
  widget()->IsVisibleOnAllWorkspaces();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnGestureEvent) {
  ui::GestureEvent event =
      CreateTestGestureEvent(ui::EventType::kGestureScrollBegin, 5, 5);
  widget()->OnGestureEvent(&event);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnKeyEvent) {
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_RIGHT, ui::EF_NONE);
  widget()->OnKeyEvent(&event);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnMouseCaptureLost) {
  widget()->OnMouseCaptureLost();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnMouseEvent) {
  gfx::Point p(200, 200);
  ui::MouseEvent event(ui::EventType::kMouseMoved, p, p, ui::EventTimeForNow(),
                       ui::EF_NONE, ui::EF_NONE);
  widget()->OnMouseEvent(&event);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnNativeBlur) {
  widget()->OnNativeBlur();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnNativeFocus) {
  widget()->OnNativeFocus();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnNativeThemeUpdated) {
  ui::TestNativeTheme theme;
  widget()->OnNativeThemeUpdated(&theme);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetActivationChanged) {
  widget()->OnNativeWidgetActivationChanged(false);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetAddedToCompositor) {
  widget()->OnNativeWidgetAddedToCompositor();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetBeginUserBoundsChange) {
  widget()->OnNativeWidgetBeginUserBoundsChange();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnNativeWidgetCreated) {
  EXPECT_DCHECK_DEATH(widget()->OnNativeWidgetCreated());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetDestroyed) {
  widget()->OnNativeWidgetDestroyed();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetDestroying) {
  EXPECT_DCHECK_DEATH(widget()->OnNativeWidgetDestroying());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetEndUserBoundsChange) {
  widget()->OnNativeWidgetEndUserBoundsChange();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnNativeWidgetMove) {
  widget()->OnNativeWidgetMove();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnNativeWidgetPaint) {
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
  widget()->OnNativeWidgetPaint(
      ui::PaintContext(display_list.get(), 1, gfx::Rect(), false));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetParentChanged) {
  widget()->OnNativeWidgetParentChanged(nullptr);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetRemovingFromCompositor) {
  widget()->OnNativeWidgetRemovingFromCompositor();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetSizeChanged) {
  widget()->OnNativeWidgetSizeChanged(gfx::Size());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetVisibilityChanged) {
  widget()->OnNativeWidgetVisibilityChanged(false);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetWindowShowStateChanged) {
  widget()->OnNativeWidgetWindowShowStateChanged();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnNativeWidgetWorkspaceChanged) {
  widget()->OnNativeWidgetWorkspaceChanged();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnOwnerClosing) {
  widget()->OnOwnerClosing();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnParentShouldPaintAsActiveChanged) {
  widget()->OnParentShouldPaintAsActiveChanged();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, OnScrollEvent) {
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(65, 5),
                         ui::EventTimeForNow(), 0, 0, 20, 0, 20, 2);
  widget()->OnScrollEvent(&scroll);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       OnSizeConstraintsChanged) {
  widget()->OnSizeConstraintsChanged();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, LayerTreeChanged) {
  widget()->LayerTreeChanged();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       LayoutRootViewIfNecessary) {
  widget()->LayoutRootViewIfNecessary();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, LockPaintAsActive) {
  widget()->LockPaintAsActive();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Maximize) {
  widget()->Maximize();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Minimize) {
  widget()->Minimize();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, movement_disabled) {
  widget()->movement_disabled();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, native_widget_private) {
  widget()->native_widget_private();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, native_widget) {
  widget()->native_widget();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, non_client_view) {
  widget()->non_client_view();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       NotifyNativeViewHierarchyChanged) {
  widget()->NotifyNativeViewHierarchyChanged();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       NotifyNativeViewHierarchyWillChange) {
  widget()->NotifyNativeViewHierarchyWillChange();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, NotifyWillRemoveView) {
  widget()->NotifyWillRemoveView(widget()->non_client_view());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, parent) {
  widget()->parent();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       RegisterPaintAsActiveChangedCallback) {
  auto subscription =
      widget()->RegisterPaintAsActiveChangedCallback(base::DoNothing());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ReleaseCapture) {
  widget()->ReleaseCapture();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ReorderNativeViews) {
  widget()->ReorderNativeViews();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ReparentNativeView) {
  EXPECT_DCHECK_DEATH(
      Widget::ReparentNativeView(widget()->GetNativeView(), nullptr));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Restore) {
  widget()->Restore();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, RunMoveLoop) {
  widget()->RunMoveLoop(gfx::Vector2d(), Widget::MoveLoopSource::kMouse,
                        Widget::MoveLoopEscapeBehavior::kHide);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, RunShellDrag) {
  std::unique_ptr<OSExchangeData> data(std::make_unique<OSExchangeData>());
  widget()->RunShellDrag(nullptr, std::move(data), gfx::Point(), 0,
                         ui::mojom::DragEventSource::kMouse);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ScheduleLayout) {
  widget()->ScheduleLayout();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SchedulePaintInRect) {
  widget()->SchedulePaintInRect(gfx::Rect(0, 0, 1, 2));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetAspectRatio) {
  widget()->SetAspectRatio(gfx::SizeF(1.0, 1.0));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetBounds) {
  widget()->SetBounds(gfx::Rect(0, 0, 100, 80));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetBoundsConstrained) {
  widget()->SetBoundsConstrained(gfx::Rect(0, 0, 120, 140));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SetCanAppearInExistingFullscreenSpaces) {
  widget()->SetCanAppearInExistingFullscreenSpaces(false);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetCapture) {
  widget()->SetCapture(widget()->GetRootView());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetContentsView) {
  View view;
  EXPECT_DCHECK_DEATH(widget()->SetContentsView(std::make_unique<View>()));
  EXPECT_DCHECK_DEATH(widget()->SetContentsView(&view));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetCursor) {
  widget()->SetCursor(ui::Cursor());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SetFocusTraversableParent) {
  std::unique_ptr<Widget> another_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget()->SetFocusTraversableParent(another_widget->GetFocusTraversable());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SetFocusTraversableParentView) {
  std::unique_ptr<Widget> another_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget()->SetFocusTraversableParentView(another_widget->GetContentsView());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetFullscreen) {
  widget()->SetFullscreen(true);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetInitialFocus) {
  widget()->SetInitialFocus(ui::mojom::WindowShowState::kInactive);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SetNativeWindowProperty) {
  widget()->SetNativeWindowProperty("xx", widget());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetOpacity) {
  widget()->SetOpacity(0.f);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetShape) {
  auto rects = std::make_unique<Widget::ShapeRects>();
  rects->emplace_back(40, 0, 20, 100);
  rects->emplace_back(0, 40, 100, 20);
  widget()->SetShape(std::move(rects));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetSize) {
  widget()->SetSize(gfx::Size(10, 11));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SetVisibilityChangedAnimationsEnabled) {
  widget()->SetVisibilityChangedAnimationsEnabled(false);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SetVisibilityAnimationDuration) {
  widget()->SetVisibilityAnimationDuration(base::Seconds(1));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SetVisibilityAnimationTransition) {
  widget()->SetVisibilityAnimationTransition(Widget::ANIMATE_BOTH);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, SetVisible) {
  widget()->SetVisibilityAnimationTransition(Widget::ANIMATE_BOTH);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SetVisibleOnAllWorkspaces) {
  widget()->SetVisibleOnAllWorkspaces(true);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       ShouldDescendIntoChildForEventHandling) {
  widget()->ShouldDescendIntoChildForEventHandling(nullptr, gfx::NativeView(),
                                                   nullptr, gfx::Point(0, 0));
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       ShouldHandleNativeWidgetActivationChanged) {
  widget()->ShouldHandleNativeWidgetActivationChanged(true);
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ShouldPaintAsActive) {
  widget()->ShouldPaintAsActive();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ShouldUseNativeFrame) {
  widget()->ShouldUseNativeFrame();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       ShouldWindowContentsBeTransparent) {
  widget()->ShouldWindowContentsBeTransparent();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, Show) {
  widget()->Show();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ShowEmojiPanel) {
  widget()->ShowEmojiPanel();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ShowInactive) {
  widget()->ShowInactive();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, StackAbove) {
  std::unique_ptr<Widget> another_widget =
      CreateTestWidget(std::get<Widget::InitParams::Ownership>(GetParam()));
  widget()->StackAbove(another_widget->GetNativeView());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, StackAboveWidget) {
  std::unique_ptr<Widget> another_widget =
      CreateTestWidget(std::get<Widget::InitParams::Ownership>(GetParam()));
  widget()->StackAboveWidget(another_widget.get());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, StackAtTop) {
  widget()->StackAtTop();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest,
       SynthesizeMouseMoveEvent) {
  widget()->SynthesizeMouseMoveEvent();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ThemeChanged) {
  widget()->ThemeChanged();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, UnlockPaintAsActive) {
  // UnlockPaintAsActive() is called in the destructor of PaintAsActiveLock.
  // External invocation is not allowed.
  EXPECT_DCHECK_DEATH(widget()->UnlockPaintAsActive());
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, UpdateWindowIcon) {
  widget()->UpdateWindowIcon();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, UpdateWindowTitle) {
  widget()->UpdateWindowTitle();
}

TEST_P(WidgetWithDestroyedNativeViewOrNativeWidgetTest, ViewHierarchyChanged) {
  widget()->ViewHierarchyChanged(
      ViewHierarchyChangedDetails(true, nullptr, nullptr, nullptr));
}

INSTANTIATE_TEST_SUITE_P(
    PlatformWidgetWithDestroyedNativeViewOrNativeWidgetTest,
    WidgetWithDestroyedNativeViewOrNativeWidgetTest,
    ::testing::Combine(
        ::testing::Values(ViewsTestBase::NativeWidgetType::kDefault,
                          ViewsTestBase::NativeWidgetType::kDesktop),
        ::testing::Values(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                          Widget::InitParams::CLIENT_OWNS_WIDGET)),
    WidgetWithDestroyedNativeViewOrNativeWidgetTest::PrintTestName);

////////////////////////////////////////////////////////////////////////////////
// Widget observer tests.
//

class WidgetObserverTest : public WidgetTest, public WidgetObserver {
 public:
  WidgetObserverTest() = default;
  ~WidgetObserverTest() override = default;

  // Set a widget to Close() the next time the Widget being observed is hidden.
  void CloseOnNextHide(Widget* widget) { widget_to_close_on_hide_ = widget; }

  // Overridden from WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override {
    if (active_ == widget) {
      active_ = nullptr;
    }
    if (widget_activated_ == widget) {
      widget_activated_ = nullptr;
    }
  }

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active) {
      if (widget_activated_) {
        widget_activated_->Deactivate();
      }
      widget_activated_ = widget;
      active_ = widget;
    } else {
      if (widget_activated_ == widget) {
        widget_activated_ = nullptr;
      }
      widget_deactivated_ = widget->GetName();
    }
  }

  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    if (visible) {
      widget_shown_ = widget->GetName();
      return;
    }
    widget_hidden_ = widget->GetName();
    if (widget_to_close_on_hide_) {
      std::exchange(widget_to_close_on_hide_, nullptr)->Close();
    }
  }

  void OnWidgetBoundsChanged(Widget* widget,
                             const gfx::Rect& new_bounds) override {
    widget_bounds_changed_ = widget->GetName();
  }

  void reset() {
    active_ = nullptr;
    widget_activated_ = nullptr;
    widget_deactivated_.clear();
    widget_shown_.clear();
    widget_hidden_.clear();
    widget_bounds_changed_.clear();
  }

  Widget* NewWidget(std::string name) {
    Widget* widget = new Widget();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.name = std::move(name);
    widget->Init(std::move(params));
    widget->AddObserver(this);
    return widget;
  }

  const Widget* active() const { return active_; }
  const Widget* widget_activated() const { return widget_activated_; }
  const std::string& widget_deactivated() const { return widget_deactivated_; }
  const std::string& widget_shown() const { return widget_shown_; }
  const std::string& widget_hidden() const { return widget_hidden_; }
  const std::string& widget_bounds_changed() const {
    return widget_bounds_changed_;
  }

 private:
  raw_ptr<Widget> active_ = nullptr;
  raw_ptr<Widget> widget_activated_ = nullptr;

  std::string widget_deactivated_;
  std::string widget_shown_;
  std::string widget_hidden_;
  std::string widget_bounds_changed_;

  raw_ptr<Widget> widget_to_close_on_hide_ = nullptr;
};

// This test appears to be flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ActivationChange DISABLED_ActivationChange
#else
#define MAYBE_ActivationChange ActivationChange
#endif

TEST_F(WidgetObserverTest, MAYBE_ActivationChange) {
  WidgetAutoclosePtr toplevel1(NewWidget("top1"));
  WidgetAutoclosePtr toplevel2(NewWidget("top2"));

  toplevel1->Show();
  toplevel2->Show();
  reset();

  toplevel1->Activate();
  RunPendingMessages();
  EXPECT_EQ(toplevel1.get(), widget_activated());

  toplevel2->Activate();
  RunPendingMessages();
  EXPECT_EQ(toplevel1->GetName(), widget_deactivated());
  EXPECT_EQ(toplevel2.get(), widget_activated());
  EXPECT_EQ(toplevel2.get(), active());
}

namespace {

// This class simulates a focus manager that moves focus to a second widget when
// the first one is closed. It simulates a situation where a sequence of widget
// observers might try to call Widget::Close in response to a OnWidgetClosing().
class WidgetActivationForwarder : public TestWidgetObserver {
 public:
  WidgetActivationForwarder(Widget* current_active_widget,
                            Widget* widget_to_activate)
      : TestWidgetObserver(current_active_widget),
        widget_to_activate_(widget_to_activate) {}

  WidgetActivationForwarder(const WidgetActivationForwarder&) = delete;
  WidgetActivationForwarder& operator=(const WidgetActivationForwarder&) =
      delete;

  ~WidgetActivationForwarder() override = default;

 private:
  // WidgetObserver overrides:
  void OnWidgetClosing(Widget* widget) override {
    widget->OnNativeWidgetActivationChanged(false);
    widget_to_activate_->Activate();
  }
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (!active) {
      widget->Close();
    }
  }

  raw_ptr<Widget> widget_to_activate_;
};

// This class observes a widget and counts the number of times OnWidgetClosing
// is called.
class WidgetCloseCounter : public TestWidgetObserver {
 public:
  explicit WidgetCloseCounter(Widget* widget) : TestWidgetObserver(widget) {}

  WidgetCloseCounter(const WidgetCloseCounter&) = delete;
  WidgetCloseCounter& operator=(const WidgetCloseCounter&) = delete;

  ~WidgetCloseCounter() override = default;

  int close_count() const { return close_count_; }

 private:
  // WidgetObserver overrides:
  void OnWidgetClosing(Widget* widget) override { close_count_++; }

  int close_count_ = 0;
};

}  // namespace

// Makes sure close notifications aren't sent more than once when a Widget is
// shutting down. Test for crbug.com/714334
TEST_F(WidgetObserverTest, CloseReentrancy) {
  Widget* widget1 = CreateTopLevelPlatformWidget();
  Widget* widget2 = CreateTopLevelPlatformWidget();
  WidgetCloseCounter counter(widget1);
  WidgetActivationForwarder focus_manager(widget1, widget2);
  widget1->Close();
  EXPECT_EQ(1, counter.close_count());
  widget2->Close();
}

TEST_F(WidgetObserverTest, VisibilityChange) {
  WidgetAutoclosePtr toplevel(CreateTopLevelPlatformWidget());
  WidgetAutoclosePtr child1(NewWidget("child1"));
  WidgetAutoclosePtr child2(NewWidget("child2"));

  toplevel->Show();
  child1->Show();
  child2->Show();

  reset();

  child1->Hide();
  EXPECT_EQ(child1->GetName(), widget_hidden());

  child2->Hide();
  EXPECT_EQ(child2->GetName(), widget_hidden());

  child1->Show();
  EXPECT_EQ(child1->GetName(), widget_shown());

  child2->Show();
  EXPECT_EQ(child2->GetName(), widget_shown());
}

TEST_F(WidgetObserverTest, DestroyBubble) {
  // This test expect NativeWidgetAura, force its creation.
  ViewsDelegate::GetInstance()->set_native_widget_factory(
      ViewsDelegate::NativeWidgetFactory());

  std::unique_ptr<Widget> anchor = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  anchor->Show();

  auto bubble_delegate =
      std::make_unique<TestBubbleDialogDelegateView>(anchor->client_view());
  {
    std::unique_ptr<Widget> bubble_widget =
        base::WrapUnique(BubbleDialogDelegateView::CreateBubble(
            bubble_delegate.release(), Widget::InitParams::CLIENT_OWNS_WIDGET));
    bubble_widget->Show();
  }

  anchor->Hide();
}

TEST_F(WidgetObserverTest, WidgetBoundsChanged) {
  WidgetAutoclosePtr child1(NewWidget("child1"));
  WidgetAutoclosePtr child2(NewWidget("child2"));

  child1->OnNativeWidgetMove();
  EXPECT_EQ(child1->GetName(), widget_bounds_changed());

  child2->OnNativeWidgetMove();
  EXPECT_EQ(child2->GetName(), widget_bounds_changed());

  child1->OnNativeWidgetSizeChanged(gfx::Size());
  EXPECT_EQ(child1->GetName(), widget_bounds_changed());

  child2->OnNativeWidgetSizeChanged(gfx::Size());
  EXPECT_EQ(child2->GetName(), widget_bounds_changed());
}

// An extension to WidgetBoundsChanged to ensure notifications are forwarded
// by the NativeWidget implementation.
TEST_F(WidgetObserverTest, WidgetBoundsChangedNative) {
  // Don't use NewWidget(), so that the Init() flow can be observed to ensure
  // consistency across platforms.
  auto widget = std::make_unique<Widget>();
  widget->AddObserver(this);

  EXPECT_THAT(widget_bounds_changed(), IsEmpty());

  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);

  // Use an origin within the work area since platforms (e.g. Mac) may move a
  // window into the work area when showing, triggering a bounds change.
  params.bounds = gfx::Rect(50, 50, 100, 100);
  params.name = "widget";

  // Init causes a bounds change, even while not showing. Note some platforms
  // cause a bounds change even when the bounds are empty. Mac does not.
  widget->Init(std::move(params));
  EXPECT_THAT(widget_bounds_changed(), Not(IsEmpty()));
  reset();

  // Resizing while hidden, triggers a change.
  widget->SetSize(gfx::Size(160, 100));
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_THAT(widget_bounds_changed(), Not(IsEmpty()));
  reset();

  // Setting the same size does nothing.
  widget->SetSize(gfx::Size(160, 100));
  EXPECT_THAT(widget_bounds_changed(), IsEmpty());
  reset();

  // Showing does nothing to the bounds.
  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_THAT(widget_bounds_changed(), IsEmpty());
  reset();

  // Resizing while shown.
  widget->SetSize(gfx::Size(170, 100));
  EXPECT_THAT(widget_bounds_changed(), Not(IsEmpty()));
  reset();

  // Resize to the same thing while shown does nothing.
  widget->SetSize(gfx::Size(170, 100));
  EXPECT_THAT(widget_bounds_changed(), IsEmpty());
  reset();

  // Move, but don't change the size.
  widget->SetBounds(gfx::Rect(110, 110, 170, 100));
  EXPECT_THAT(widget_bounds_changed(), Not(IsEmpty()));
  reset();

  // Moving to the same place does nothing.
  widget->SetBounds(gfx::Rect(110, 110, 170, 100));
  EXPECT_THAT(widget_bounds_changed(), IsEmpty());
  reset();

  // No bounds change when closing.
  std::exchange(widget, nullptr)->CloseNow();
  EXPECT_THAT(widget_bounds_changed(), IsEmpty());
}

namespace {

class MoveTrackingTestDesktopWidgetDelegate : public TestDesktopWidgetDelegate {
 public:
  int move_count() const { return move_count_; }

  // WidgetDelegate:
  void OnWidgetMove() override { ++move_count_; }

 private:
  int move_count_ = 0;
};

}  // namespace

class DesktopWidgetObserverTest : public WidgetObserverTest {
 public:
  DesktopWidgetObserverTest() = default;

  DesktopWidgetObserverTest(const DesktopWidgetObserverTest&) = delete;
  DesktopWidgetObserverTest& operator=(const DesktopWidgetObserverTest&) =
      delete;

  ~DesktopWidgetObserverTest() override = default;

  // WidgetObserverTest:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    WidgetObserverTest::SetUp();
  }
};

// An extension to the WidgetBoundsChangedNative test above to ensure move
// notifications propagate to the WidgetDelegate.
TEST_F(DesktopWidgetObserverTest, OnWidgetMovedWhenOriginChangesNative) {
  MoveTrackingTestDesktopWidgetDelegate delegate;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  delegate.InitWidget(std::move(params));
  Widget* widget = delegate.GetWidget();
  widget->Show();
  widget->SetBounds(gfx::Rect(100, 100, 300, 200));

  const int moves_during_init = delegate.move_count();

  // Resize without changing origin. No move.
  widget->SetBounds(gfx::Rect(100, 100, 310, 210));
  EXPECT_EQ(moves_during_init, delegate.move_count());

  // Move without changing size. Moves.
  widget->SetBounds(gfx::Rect(110, 110, 310, 210));
  EXPECT_EQ(moves_during_init + 1, delegate.move_count());

  // Changing both moves.
  widget->SetBounds(gfx::Rect(90, 90, 330, 230));
  EXPECT_EQ(moves_during_init + 2, delegate.move_count());

  // Just grow vertically. On Mac, this changes the AppKit origin since it is
  // from the bottom left of the screen, but there is no move as far as views is
  // concerned.
  widget->SetBounds(gfx::Rect(90, 90, 330, 240));
  // No change.
  EXPECT_EQ(moves_during_init + 2, delegate.move_count());

  // For a similar reason, move the widget down by the same amount that it grows
  // vertically. The AppKit origin does not change, but it is a move.
  widget->SetBounds(gfx::Rect(90, 100, 330, 250));
  EXPECT_EQ(moves_during_init + 3, delegate.move_count());
}

// Test correct behavior when widgets close themselves in response to visibility
// changes.
TEST_F(WidgetObserverTest, ClosingOnHiddenParent) {
  WidgetAutoclosePtr parent(NewWidget("parent"));
  Widget* child = CreateChildPlatformWidget(parent->GetNativeView());

  TestWidgetObserver child_observer(child);

  EXPECT_FALSE(parent->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  // Note |child| is TYPE_CONTROL, which start shown. So no need to show the
  // child separately.
  parent->Show();
  EXPECT_TRUE(parent->IsVisible());
  EXPECT_TRUE(child->IsVisible());

  // Simulate a child widget that closes itself when the parent is hidden.
  CloseOnNextHide(child);
  EXPECT_FALSE(child_observer.widget_closed());
  parent->Hide();
  RunPendingMessages();
  EXPECT_TRUE(child_observer.widget_closed());
}

// Test behavior of NativeWidget*::GetWindowPlacement on the native desktop.
#if BUILDFLAG(IS_LINUX)
// On desktop-Linux cheat and use non-desktop widgets. On X11, minimize is
// asynchronous. Also (harder) showing a window doesn't activate it without
// user interaction (or extra steps only done for interactive ui tests).
// Without that, show_state remains in ui::mojom::WindowShowState::kInactive
// throughout.
// TODO(tapted): Find a nice way to run this with desktop widgets on Linux.
TEST_F(WidgetTest, GetWindowPlacement) {
#else
TEST_F(DesktopWidgetTest, GetWindowPlacement) {
#endif
  WidgetAutoclosePtr widget;
  widget.reset(CreateTopLevelNativeWidget());

  gfx::Rect expected_bounds(100, 110, 200, 220);
  widget->SetBounds(expected_bounds);
  widget->Show();

  // Start with something invalid to ensure it changes.
  ui::mojom::WindowShowState show_state = ui::mojom::WindowShowState::kEnd;
  gfx::Rect restored_bounds;

  internal::NativeWidgetPrivate* native_widget =
      widget->native_widget_private();

  native_widget->GetWindowPlacement(&restored_bounds, &show_state);
  EXPECT_EQ(expected_bounds, restored_bounds);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Non-desktop/Ash widgets start off in "default" until a Restore().
  EXPECT_EQ(ui::mojom::WindowShowState::kDefault, show_state);
  widget->Restore();
  native_widget->GetWindowPlacement(&restored_bounds, &show_state);
#endif
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal, show_state);

  views::test::PropertyWaiter minimize_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget.get())),
      true);
  widget->Minimize();
  EXPECT_TRUE(minimize_waiter.Wait());

  native_widget->GetWindowPlacement(&restored_bounds, &show_state);
  EXPECT_EQ(ui::mojom::WindowShowState::kMinimized, show_state);
  EXPECT_EQ(expected_bounds, restored_bounds);

  views::test::PropertyWaiter restore_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget.get())),
      false);
  widget->Restore();
  EXPECT_TRUE(restore_waiter.Wait());

  native_widget->GetWindowPlacement(&restored_bounds, &show_state);
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal, show_state);
  EXPECT_EQ(expected_bounds, restored_bounds);

  expected_bounds = gfx::Rect(130, 140, 230, 250);
  widget->SetBounds(expected_bounds);
  native_widget->GetWindowPlacement(&restored_bounds, &show_state);
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal, show_state);
  EXPECT_EQ(expected_bounds, restored_bounds);

  widget->SetFullscreen(true);
  native_widget->GetWindowPlacement(&restored_bounds, &show_state);

#if BUILDFLAG(IS_WIN)
  // Desktop Aura widgets on Windows currently don't update show_state when
  // going fullscreen, and report restored_bounds as the full screen size.
  // See http://crbug.com/475813.
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal, show_state);
#else
  EXPECT_EQ(ui::mojom::WindowShowState::kFullscreen, show_state);
  EXPECT_EQ(expected_bounds, restored_bounds);
#endif

  widget->SetFullscreen(false);
  native_widget->GetWindowPlacement(&restored_bounds, &show_state);
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal, show_state);
  EXPECT_EQ(expected_bounds, restored_bounds);
}

// Test that widget size constraints are properly applied immediately after
// Init(), and that SetBounds() calls are appropriately clamped.
TEST_F(DesktopWidgetTest, MinimumSizeConstraints) {
  TestDesktopWidgetDelegate delegate;
  gfx::Size minimum_size(100, 100);
  const gfx::Size smaller_size(90, 90);

  delegate.set_contents_view(new StaticSizedView(minimum_size));
  delegate.InitWidget(CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                   Widget::InitParams::TYPE_WINDOW));
  Widget* widget = delegate.GetWidget();

  // On desktop Linux, the Widget must be shown to ensure the window is mapped.
  // On other platforms this line is optional.
  widget->Show();

  // Sanity checks.
  EXPECT_GT(delegate.initial_bounds().width(), minimum_size.width());
  EXPECT_GT(delegate.initial_bounds().height(), minimum_size.height());
  EXPECT_EQ(delegate.initial_bounds().size(),
            widget->GetWindowBoundsInScreen().size());
  // Note: StaticSizedView doesn't currently provide a maximum size.
  EXPECT_EQ(gfx::Size(), widget->GetMaximumSize());

  if (!widget->ShouldUseNativeFrame()) {
    // The test environment may have dwm disabled on Windows. In this case,
    // CustomFrameView is used instead of the NativeFrameView, which will
    // provide a minimum size that includes frame decorations.
    minimum_size = widget->non_client_view()
                       ->GetWindowBoundsForClientBounds(gfx::Rect(minimum_size))
                       .size();
  }

  EXPECT_EQ(minimum_size, widget->GetMinimumSize());
  EXPECT_EQ(minimum_size, GetNativeWidgetMinimumContentSize(widget));

  // Trying to resize smaller than the minimum size should restrict the content
  // size to the minimum size.
  widget->SetBounds(gfx::Rect(smaller_size));
  EXPECT_EQ(minimum_size, widget->GetClientAreaBoundsInScreen().size());

  widget->SetSize(smaller_size);
  EXPECT_EQ(minimum_size, widget->GetClientAreaBoundsInScreen().size());
}

// When a non-desktop widget has a desktop child widget, due to the
// async nature of desktop widget shutdown, the parent can be destroyed before
// its child. Make sure that parent() returns nullptr at this time.
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
TEST_F(DesktopWidgetTest, GetPossiblyDestroyedParent) {
  WidgetAutoclosePtr root(CreateTopLevelNativeWidget());

  const auto create_widget = [](Widget* parent, bool is_desktop) {
    Widget* widget = new Widget;
    Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW);
    init_params.parent = parent->GetNativeView();
    init_params.context = parent->GetNativeView();
    if (is_desktop) {
      init_params.native_widget =
          new test::TestPlatformNativeWidget<DesktopNativeWidgetAura>(
              widget, false, nullptr);
    } else {
      init_params.native_widget =
          new test::TestPlatformNativeWidget<NativeWidgetAura>(widget, false,
                                                               nullptr);
    }
    widget->Init(std::move(init_params));
    return widget;
  };

  WidgetAutoclosePtr child(create_widget(root.get(), /* non-desktop */ false));
  WidgetAutoclosePtr grandchild(create_widget(child.get(), /* desktop */ true));

  child.reset();
  EXPECT_EQ(grandchild->parent(), nullptr);
}
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)

// Tests that SetBounds() and GetWindowBoundsInScreen() is symmetric when the
// widget is visible and not maximized or fullscreen.
TEST_F(WidgetTest, GetWindowBoundsInScreen) {
  // Choose test coordinates away from edges and dimensions that are "small"
  // (but not too small) to ensure the OS doesn't try to adjust them.
  const gfx::Rect kTestBounds(150, 150, 400, 300);
  const gfx::Size kTestSize(200, 180);

  {
    // First test a toplevel widget.
    WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
    widget->Show();

    EXPECT_NE(kTestSize.ToString(),
              widget->GetWindowBoundsInScreen().size().ToString());
    widget->SetSize(kTestSize);
    EXPECT_EQ(kTestSize.ToString(),
              widget->GetWindowBoundsInScreen().size().ToString());

    EXPECT_NE(kTestBounds.ToString(),
              widget->GetWindowBoundsInScreen().ToString());
    widget->SetBounds(kTestBounds);
    EXPECT_EQ(kTestBounds.ToString(),
              widget->GetWindowBoundsInScreen().ToString());

    // Changing just the size should not change the origin.
    widget->SetSize(kTestSize);
    EXPECT_EQ(kTestBounds.origin().ToString(),
              widget->GetWindowBoundsInScreen().origin().ToString());
  }

  // Same tests with a frameless window.
  WidgetAutoclosePtr widget(CreateTopLevelFramelessPlatformWidget());
  widget->Show();

  EXPECT_NE(kTestSize.ToString(),
            widget->GetWindowBoundsInScreen().size().ToString());
  widget->SetSize(kTestSize);
  EXPECT_EQ(kTestSize.ToString(),
            widget->GetWindowBoundsInScreen().size().ToString());

  EXPECT_NE(kTestBounds.ToString(),
            widget->GetWindowBoundsInScreen().ToString());
  widget->SetBounds(kTestBounds);
  EXPECT_EQ(kTestBounds.ToString(),
            widget->GetWindowBoundsInScreen().ToString());

  // For a frameless widget, the client bounds should also match.
  EXPECT_EQ(kTestBounds.ToString(),
            widget->GetClientAreaBoundsInScreen().ToString());

  // Verify origin is stable for a frameless window as well.
  widget->SetSize(kTestSize);
  EXPECT_EQ(kTestBounds.origin().ToString(),
            widget->GetWindowBoundsInScreen().origin().ToString());
}

// Chrome OS widgets need the shell to maximize/fullscreen window.
// Disable on desktop Linux because windows restore to the wrong bounds.
// See http://crbug.com/515369.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_GetRestoredBounds DISABLED_GetRestoredBounds
#else
#define MAYBE_GetRestoredBounds GetRestoredBounds
#endif

// Test that GetRestoredBounds() returns the original bounds of the window.
TEST_F(DesktopWidgetTest, MAYBE_GetRestoredBounds) {
  WidgetAutoclosePtr toplevel(CreateTopLevelNativeWidget());
  toplevel->Show();
  // Initial restored bounds have non-zero size.
  EXPECT_FALSE(toplevel->GetRestoredBounds().IsEmpty());

  const gfx::Rect bounds(100, 100, 200, 200);
  toplevel->SetBounds(bounds);
  EXPECT_EQ(bounds, toplevel->GetWindowBoundsInScreen());
  EXPECT_EQ(bounds, toplevel->GetRestoredBounds());

  toplevel->Maximize();
  RunPendingMessages();
#if BUILDFLAG(IS_MAC)
  // Current expectation on Mac is to do nothing on Maximize.
  EXPECT_EQ(toplevel->GetWindowBoundsInScreen(), toplevel->GetRestoredBounds());
#else
  EXPECT_NE(toplevel->GetWindowBoundsInScreen(), toplevel->GetRestoredBounds());
#endif
  EXPECT_EQ(bounds, toplevel->GetRestoredBounds());

  toplevel->Restore();
  RunPendingMessages();
  EXPECT_EQ(bounds, toplevel->GetWindowBoundsInScreen());
  EXPECT_EQ(bounds, toplevel->GetRestoredBounds());

  toplevel->SetFullscreen(true);
  RunPendingMessages();

  EXPECT_NE(toplevel->GetWindowBoundsInScreen(), toplevel->GetRestoredBounds());
  EXPECT_EQ(bounds, toplevel->GetRestoredBounds());

  toplevel->SetFullscreen(false);
  RunPendingMessages();
  EXPECT_EQ(bounds, toplevel->GetWindowBoundsInScreen());
  EXPECT_EQ(bounds, toplevel->GetRestoredBounds());
}

// The key-event propagation from Widget happens differently on aura and
// non-aura systems because of the difference in IME. So this test works only on
// aura.
TEST_F(WidgetTest, KeyboardInputEvent) {
  WidgetAutoclosePtr toplevel(CreateTopLevelPlatformWidget());
  View* container = toplevel->client_view();

  Textfield* textfield = new Textfield();
  textfield->SetText(u"some text");
  container->AddChildView(textfield);
  toplevel->Show();
  textfield->RequestFocus();

  // The press gets handled. The release doesn't have an effect.
  ui::KeyEvent backspace_p(ui::EventType::kKeyPressed, ui::VKEY_DELETE,
                           ui::EF_NONE);
  toplevel->OnKeyEvent(&backspace_p);
  EXPECT_TRUE(backspace_p.stopped_propagation());
  ui::KeyEvent backspace_r(ui::EventType::kKeyReleased, ui::VKEY_DELETE,
                           ui::EF_NONE);
  toplevel->OnKeyEvent(&backspace_r);
  EXPECT_FALSE(backspace_r.handled());
}

TEST_F(WidgetTest, BubbleControlsResetOnInit) {
  std::unique_ptr<Widget> anchor = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  anchor->Show();

  {
    auto bubble_delegate =
        std::make_unique<TestBubbleDialogDelegateView>(anchor->client_view());
    auto* bubble_delegate_ptr = bubble_delegate.get();
    std::unique_ptr<Widget> bubble_widget =
        base::WrapUnique(BubbleDialogDelegateView::CreateBubble(
            bubble_delegate.release(), Widget::InitParams::CLIENT_OWNS_WIDGET));
    EXPECT_TRUE(bubble_delegate_ptr->reset_controls_called_);
    bubble_widget->Show();
  }

  anchor->Hide();
}

#if BUILDFLAG(IS_WIN)
// Test to ensure that after minimize, view width is set to zero. This is only
// the case for desktop widgets on Windows. Other platforms retain the window
// size while minimized.
TEST_F(DesktopWidgetTest, TestViewWidthAfterMinimizingWidget) {
  // Create a widget.
  std::unique_ptr<Widget> widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  NonClientView* non_client_view = widget->non_client_view();
  non_client_view->SetFrameView(
      std::make_unique<MinimumSizeFrameView>(widget.get()));
  // Setting the frame view doesn't do a layout, so force one.
  non_client_view->InvalidateLayout();
  views::test::RunScheduledLayout(non_client_view);
  widget->Show();
  EXPECT_NE(0, non_client_view->frame_view()->width());
  widget->Minimize();
  EXPECT_EQ(0, non_client_view->frame_view()->width());
}
#endif

// Desktop native widget Aura tests are for non Chrome OS platforms.
// This class validates whether paints are received for a visible Widget.
// It observes Widget visibility and Close() and tracks whether subsequent
// paints are expected.
class DesktopAuraTestValidPaintWidget : public Widget, public WidgetObserver {
 public:
  explicit DesktopAuraTestValidPaintWidget(Widget::InitParams init_params)
      : Widget(std::move(init_params)) {
    observation_.Observe(this);
  }

  DesktopAuraTestValidPaintWidget(const DesktopAuraTestValidPaintWidget&) =
      delete;
  DesktopAuraTestValidPaintWidget& operator=(
      const DesktopAuraTestValidPaintWidget&) = delete;

  ~DesktopAuraTestValidPaintWidget() override = default;

  bool ReadReceivedPaintAndReset() {
    return std::exchange(received_paint_, false);
  }

  bool received_paint_while_hidden() const {
    return received_paint_while_hidden_;
  }

  void WaitUntilPaint() {
    if (received_paint_) {
      return;
    }
    base::RunLoop runloop;
    quit_closure_ = runloop.QuitClosure();
    runloop.Run();
    quit_closure_.Reset();
  }

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override { expect_paint_ = false; }

  void OnNativeWidgetPaint(const ui::PaintContext& context) override {
    received_paint_ = true;
    EXPECT_TRUE(expect_paint_);
    if (!expect_paint_) {
      received_paint_while_hidden_ = true;
    }
    Widget::OnNativeWidgetPaint(context);
    if (!quit_closure_.is_null()) {
      std::move(quit_closure_).Run();
    }
  }

  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    expect_paint_ = visible;
  }

 private:
  bool received_paint_ = false;
  bool expect_paint_ = true;
  bool received_paint_while_hidden_ = false;
  base::OnceClosure quit_closure_;
  base::ScopedObservation<Widget, WidgetObserver> observation_{this};
};

namespace {

class ContentsView : public View {
  METADATA_HEADER(ContentsView, View)
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetNameExplicitlyEmpty();
    // Focusable Views need a valid role.
    node_data->role = ax::mojom::Role::kDialog;
  }
};

BEGIN_METADATA(ContentsView)
END_METADATA

}  // namespace

class DesktopAuraPaintWidgetTest : public DesktopWidgetTest {
 public:
  std::unique_ptr<DesktopAuraTestValidPaintWidget>
  CreateDesktopAuraTestValidPaintWidget(
      Widget::InitParams::Type type =
          Widget::InitParams::TYPE_WINDOW_FRAMELESS) {
    auto widget = std::make_unique<DesktopAuraTestValidPaintWidget>(
        CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  type));

    View* contents_view =
        widget->SetContentsView(std::make_unique<ContentsView>());
    contents_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);

    widget->Show();
    widget->Activate();

    return widget;
  }
};

TEST_F(DesktopAuraPaintWidgetTest, DesktopNativeWidgetNoPaintAfterCloseTest) {
  auto widget = CreateDesktopAuraTestValidPaintWidget();
  widget->WaitUntilPaint();
  EXPECT_TRUE(widget->ReadReceivedPaintAndReset());
  widget->SchedulePaintInRect(widget->GetRestoredBounds());
  widget->Close();
  RunPendingMessages();
  EXPECT_FALSE(widget->ReadReceivedPaintAndReset());
  EXPECT_FALSE(widget->received_paint_while_hidden());
}

TEST_F(DesktopAuraPaintWidgetTest, DesktopNativeWidgetNoPaintAfterHideTest) {
  auto widget = CreateDesktopAuraTestValidPaintWidget();
  widget->WaitUntilPaint();
  EXPECT_TRUE(widget->ReadReceivedPaintAndReset());
  widget->SchedulePaintInRect(widget->GetRestoredBounds());
  widget->Hide();
  RunPendingMessages();
  EXPECT_FALSE(widget->ReadReceivedPaintAndReset());
  EXPECT_FALSE(widget->received_paint_while_hidden());
  widget->Close();
}

// Test to ensure that the aura Window's visibility state is set to visible if
// the underlying widget is hidden and then shown.
TEST_F(DesktopWidgetTest, TestWindowVisibilityAfterHide) {
  // Create a widget.
  std::unique_ptr<Widget> widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  NonClientView* non_client_view = widget->non_client_view();
  non_client_view->SetFrameView(
      std::make_unique<MinimumSizeFrameView>(widget.get()));

  widget->Show();
  EXPECT_TRUE(IsNativeWindowVisible(widget->GetNativeWindow()));
  widget->Hide();
  EXPECT_FALSE(IsNativeWindowVisible(widget->GetNativeWindow()));
  widget->Show();
  EXPECT_TRUE(IsNativeWindowVisible(widget->GetNativeWindow()));
}

// Tests that wheel events generated from scroll events are targeted to the
// views under the cursor when the focused view does not processed them.
TEST_F(WidgetTest, WheelEventsFromScrollEventTarget) {
  EventCountView* cursor_view = new EventCountView;
  cursor_view->SetBounds(60, 0, 50, 40);

  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->GetRootView()->AddChildView(cursor_view);

  // Generate a scroll event on the cursor view.
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(65, 5),
                         ui::EventTimeForNow(), 0, 0, 20, 0, 20, 2);
  widget->OnScrollEvent(&scroll);

  EXPECT_EQ(1, cursor_view->GetEventCount(ui::EventType::kScroll));
  EXPECT_EQ(1, cursor_view->GetEventCount(ui::EventType::kMousewheel));

  cursor_view->ResetCounts();

  ui::ScrollEvent scroll2(ui::EventType::kScroll, gfx::Point(5, 5),
                          ui::EventTimeForNow(), 0, 0, 20, 0, 20, 2);
  widget->OnScrollEvent(&scroll2);

  EXPECT_EQ(0, cursor_view->GetEventCount(ui::EventType::kScroll));
  EXPECT_EQ(0, cursor_view->GetEventCount(ui::EventType::kMousewheel));
}

// Tests that if a scroll-begin gesture is not handled, then subsequent scroll
// events are not dispatched to any view.
TEST_F(WidgetTest, GestureScrollEventDispatching) {
  EventCountView* noscroll_view = new EventCountView;
  EventCountView* scroll_view = new ScrollableEventCountView;

  noscroll_view->SetBounds(0, 0, 50, 40);
  scroll_view->SetBounds(60, 0, 40, 40);

  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->GetRootView()->AddChildView(noscroll_view);
  widget->GetRootView()->AddChildView(scroll_view);

  {
    ui::GestureEvent begin =
        CreateTestGestureEvent(ui::EventType::kGestureScrollBegin, 5, 5);
    widget->OnGestureEvent(&begin);
    ui::GestureEvent update = CreateTestGestureEvent(
        ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 20, 10),
        25, 15);
    widget->OnGestureEvent(&update);
    ui::GestureEvent end =
        CreateTestGestureEvent(ui::EventType::kGestureScrollEnd, 25, 15);
    widget->OnGestureEvent(&end);

    EXPECT_EQ(1,
              noscroll_view->GetEventCount(ui::EventType::kGestureScrollBegin));
    EXPECT_EQ(
        0, noscroll_view->GetEventCount(ui::EventType::kGestureScrollUpdate));
    EXPECT_EQ(0,
              noscroll_view->GetEventCount(ui::EventType::kGestureScrollEnd));
  }

  {
    ui::GestureEvent begin =
        CreateTestGestureEvent(ui::EventType::kGestureScrollBegin, 65, 5);
    widget->OnGestureEvent(&begin);
    ui::GestureEvent update = CreateTestGestureEvent(
        ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 20, 10),
        85, 15);
    widget->OnGestureEvent(&update);
    ui::GestureEvent end =
        CreateTestGestureEvent(ui::EventType::kGestureScrollEnd, 85, 15);
    widget->OnGestureEvent(&end);

    EXPECT_EQ(1,
              scroll_view->GetEventCount(ui::EventType::kGestureScrollBegin));
    EXPECT_EQ(1,
              scroll_view->GetEventCount(ui::EventType::kGestureScrollUpdate));
    EXPECT_EQ(1, scroll_view->GetEventCount(ui::EventType::kGestureScrollEnd));
  }
}

// Tests that event-handlers installed on the RootView get triggered correctly.
// TODO(tdanderson): Clean up this test as part of crbug.com/355680.
TEST_F(WidgetTest, EventHandlersOnRootView) {
  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());
  View* root_view = widget->GetRootView();

  EventCountView* view =
      root_view->AddChildView(std::make_unique<EventCountView>());
  view->SetBounds(0, 0, 20, 20);

  EventCountHandler h1;
  root_view->AddPreTargetHandler(&h1);

  EventCountHandler h2;
  root_view->AddPostTargetHandler(&h2);

  widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget->Show();

  // Dispatch a ui::EventType::kScroll event. The event remains unhandled and
  // should bubble up the views hierarchy to be re-dispatched on the root view.
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(5, 5),
                         ui::EventTimeForNow(), 0, 0, 20, 0, 20, 2);
  widget->OnScrollEvent(&scroll);
  EXPECT_EQ(2, h1.GetEventCount(ui::EventType::kScroll));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kScroll));
  EXPECT_EQ(2, h2.GetEventCount(ui::EventType::kScroll));

  // Unhandled scroll events are turned into wheel events and re-dispatched.
  EXPECT_EQ(1, h1.GetEventCount(ui::EventType::kMousewheel));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMousewheel));
  EXPECT_EQ(1, h2.GetEventCount(ui::EventType::kMousewheel));

  h1.ResetCounts();
  view->ResetCounts();
  h2.ResetCounts();

  // Dispatch a ui::EventType::kScrollFlingStart event. The event remains
  // unhandled and should bubble up the views hierarchy to be re-dispatched on
  // the root view.
  ui::ScrollEvent fling(ui::EventType::kScrollFlingStart, gfx::Point(5, 5),
                        ui::EventTimeForNow(), 0, 0, 20, 0, 20, 2);
  widget->OnScrollEvent(&fling);
  EXPECT_EQ(2, h1.GetEventCount(ui::EventType::kScrollFlingStart));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kScrollFlingStart));
  EXPECT_EQ(2, h2.GetEventCount(ui::EventType::kScrollFlingStart));

  // Unhandled scroll events which are not of type ui::EventType::kScroll should
  // not be turned into wheel events and re-dispatched.
  EXPECT_EQ(0, h1.GetEventCount(ui::EventType::kMousewheel));
  EXPECT_EQ(0, view->GetEventCount(ui::EventType::kMousewheel));
  EXPECT_EQ(0, h2.GetEventCount(ui::EventType::kMousewheel));

  h1.ResetCounts();
  view->ResetCounts();
  h2.ResetCounts();

  // Change the handle mode of |view| so that events are marked as handled at
  // the target phase.
  view->set_handle_mode(EventCountView::CONSUME_EVENTS);

  // Dispatch a ui::EventType::kGestureTapDown and a
  // ui::EventType::kGestureTapCancel event. The events are handled at the
  // target phase and should not reach the post-target handler.
  ui::GestureEvent tap_down =
      CreateTestGestureEvent(ui::EventType::kGestureTapDown, 5, 5);
  widget->OnGestureEvent(&tap_down);
  EXPECT_EQ(1, h1.GetEventCount(ui::EventType::kGestureTapDown));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kGestureTapDown));
  EXPECT_EQ(0, h2.GetEventCount(ui::EventType::kGestureTapDown));

  ui::GestureEvent tap_cancel =
      CreateTestGestureEvent(ui::EventType::kGestureTapCancel, 5, 5);
  widget->OnGestureEvent(&tap_cancel);
  EXPECT_EQ(1, h1.GetEventCount(ui::EventType::kGestureTapCancel));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kGestureTapCancel));
  EXPECT_EQ(0, h2.GetEventCount(ui::EventType::kGestureTapCancel));

  h1.ResetCounts();
  view->ResetCounts();
  h2.ResetCounts();

  // Dispatch a ui::EventType::kScroll event. The event is handled at the target
  // phase and should not reach the post-target handler.
  ui::ScrollEvent consumed_scroll(ui::EventType::kScroll, gfx::Point(5, 5),
                                  ui::EventTimeForNow(), 0, 0, 20, 0, 20, 2);
  widget->OnScrollEvent(&consumed_scroll);
  EXPECT_EQ(1, h1.GetEventCount(ui::EventType::kScroll));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kScroll));
  EXPECT_EQ(0, h2.GetEventCount(ui::EventType::kScroll));

  // Handled scroll events are not turned into wheel events and re-dispatched.
  EXPECT_EQ(0, h1.GetEventCount(ui::EventType::kMousewheel));
  EXPECT_EQ(0, view->GetEventCount(ui::EventType::kMousewheel));
  EXPECT_EQ(0, h2.GetEventCount(ui::EventType::kMousewheel));

  root_view->RemovePreTargetHandler(&h1);
}

TEST_F(WidgetTest, SynthesizeMouseMoveEvent) {
  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());
  View* root_view = widget->GetRootView();
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));

  EventCountView* v1 =
      root_view->AddChildView(std::make_unique<EventCountView>());
  v1->SetBounds(5, 5, 10, 10);
  EventCountView* v2 =
      root_view->AddChildView(std::make_unique<EventCountView>());
  v2->SetBounds(5, 15, 10, 10);

  widget->Show();

  // SynthesizeMouseMoveEvent does nothing until the mouse is entered.
  widget->SynthesizeMouseMoveEvent();
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseMoved));

  gfx::Point cursor_location(v1->GetBoundsInScreen().CenterPoint());
  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());
  generator->MoveMouseTo(cursor_location);

  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseMoved));

  // SynthesizeMouseMoveEvent dispatches an mousemove event.
  widget->SynthesizeMouseMoveEvent();
  EXPECT_EQ(2, v1->GetEventCount(ui::EventType::kMouseMoved));

  root_view->RemoveChildViewT(v1);
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseMoved));
  v2->SetBounds(5, 5, 10, 10);
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseMoved));

  widget->SynthesizeMouseMoveEvent();
  EXPECT_EQ(1, v2->GetEventCount(ui::EventType::kMouseMoved));
}

namespace {

// ui::EventHandler which handles all mouse press events.
class MousePressEventConsumer : public ui::EventHandler {
 public:
  MousePressEventConsumer() = default;

  MousePressEventConsumer(const MousePressEventConsumer&) = delete;
  MousePressEventConsumer& operator=(const MousePressEventConsumer&) = delete;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      event->SetHandled();
    }
  }
};

}  // namespace

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)

// Test that mouse presses and mouse releases are dispatched normally when a
// touch is down.
TEST_F(WidgetTest, MouseEventDispatchWhileTouchIsDown) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->Show();
  widget->SetSize(gfx::Size(300, 300));

  EventCountView* event_count_view =
      widget->GetRootView()->AddChildView(std::make_unique<EventCountView>());
  event_count_view->SetBounds(0, 0, 300, 300);

  MousePressEventConsumer consumer;
  event_count_view->AddPostTargetHandler(&consumer);

  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());
  generator->PressTouch();
  generator->ClickLeftButton();

  EXPECT_EQ(1, event_count_view->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, event_count_view->GetEventCount(ui::EventType::kMouseReleased));

  // For mus it's important we destroy the widget before the EventGenerator.
  widget->CloseNow();
}

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)

// Tests that when there is no active capture, that a mouse press causes capture
// to be set.
TEST_F(WidgetTest, MousePressCausesCapture) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->Show();
  widget->SetSize(gfx::Size(300, 300));

  EventCountView* event_count_view =
      widget->GetRootView()->AddChildView(std::make_unique<EventCountView>());
  event_count_view->SetBounds(0, 0, 300, 300);

  // No capture has been set.
  EXPECT_EQ(gfx::NativeView(), internal::NativeWidgetPrivate::GetGlobalCapture(
                                   widget->GetNativeView()));

  MousePressEventConsumer consumer;
  event_count_view->AddPostTargetHandler(&consumer);
  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());
  generator->MoveMouseTo(widget->GetClientAreaBoundsInScreen().CenterPoint());
  generator->PressLeftButton();

  EXPECT_EQ(1, event_count_view->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(
      widget->GetNativeView(),
      internal::NativeWidgetPrivate::GetGlobalCapture(widget->GetNativeView()));

  // For mus it's important we destroy the widget before the EventGenerator.
  widget->CloseNow();
}

namespace {

// An EventHandler which shows a Widget upon receiving a mouse event. The Widget
// proceeds to take capture.
class CaptureEventConsumer : public ui::EventHandler {
 public:
  explicit CaptureEventConsumer(Widget* widget) : widget_(widget) {}

  CaptureEventConsumer(const CaptureEventConsumer&) = delete;
  CaptureEventConsumer& operator=(const CaptureEventConsumer&) = delete;

  ~CaptureEventConsumer() override { widget_.ExtractAsDangling()->CloseNow(); }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      event->SetHandled();
      widget_->Show();
      widget_->SetSize(gfx::Size(200, 200));

      auto event_count_view = std::make_unique<EventCountView>();
      event_count_view->SetBounds(0, 0, 200, 200);
      widget_->SetCapture(
          widget_->GetRootView()->AddChildView(std::move(event_count_view)));
    }
  }

  raw_ptr<Widget> widget_;
};

}  // namespace

// Tests that if explicit capture occurs during a mouse press, that implicit
// capture is not applied.
TEST_F(WidgetTest, CaptureDuringMousePressNotOverridden) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->Show();
  widget->SetSize(gfx::Size(300, 300));

  EventCountView* event_count_view =
      widget->GetRootView()->AddChildView(std::make_unique<EventCountView>());
  event_count_view->SetBounds(0, 0, 300, 300);

  EXPECT_EQ(gfx::NativeView(), internal::NativeWidgetPrivate::GetGlobalCapture(
                                   widget->GetNativeView()));

  Widget* widget2 = CreateTopLevelNativeWidget();
  // Gives explicit capture to |widget2|
  CaptureEventConsumer consumer(widget2);
  event_count_view->AddPostTargetHandler(&consumer);
  auto generator =
      CreateEventGenerator(GetRootWindow(widget), widget->GetNativeWindow());
  generator->MoveMouseTo(widget->GetClientAreaBoundsInScreen().CenterPoint());
  // This event should implicitly give capture to |widget|, except that
  // |consumer| will explicitly set capture on |widget2|.
  generator->PressLeftButton();

  EXPECT_EQ(1, event_count_view->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_NE(
      widget->GetNativeView(),
      internal::NativeWidgetPrivate::GetGlobalCapture(widget->GetNativeView()));
  EXPECT_EQ(
      widget2->GetNativeView(),
      internal::NativeWidgetPrivate::GetGlobalCapture(widget->GetNativeView()));

  // For mus it's important we destroy the widget before the EventGenerator.
  widget->CloseNow();
}

class ClosingEventObserver : public ui::EventObserver {
 public:
  explicit ClosingEventObserver(Widget* widget) : widget_(widget) {}

  ClosingEventObserver(const ClosingEventObserver&) = delete;
  ClosingEventObserver& operator=(const ClosingEventObserver&) = delete;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    // Guard against attempting to close the widget twice.
    if (widget_) {
      widget_.ExtractAsDangling()->CloseNow();
    }
  }

 private:
  raw_ptr<Widget, DanglingUntriaged> widget_;
};

class ClosingView : public View {
  METADATA_HEADER(ClosingView, View)

 public:
  explicit ClosingView(Widget* widget) : widget_(widget) {}

  ClosingView(const ClosingView&) = delete;
  ClosingView& operator=(const ClosingView&) = delete;

  // View:
  void OnEvent(ui::Event* event) override {
    // Guard against closing twice and writing to freed memory.
    if (widget_ && event->type() == ui::EventType::kMousePressed) {
      Widget* widget = widget_;
      widget_ = nullptr;
      widget->CloseNow();
    }
  }

 private:
  raw_ptr<Widget> widget_;
};

BEGIN_METADATA(ClosingView)
END_METADATA

// Ensures that when multiple objects are intercepting OS-level events, that one
// can safely close a Widget that has capture.
TEST_F(WidgetTest, DestroyedWithCaptureViaEventMonitor) {
  Widget* widget = CreateTopLevelNativeWidget();
  TestWidgetObserver observer(widget);
  widget->Show();
  widget->SetSize(gfx::Size(300, 300));

  // ClosingView and ClosingEventObserver both try to close the Widget. On Mac
  // the order that EventMonitors receive OS events is not deterministic. If the
  // one installed via SetCapture() sees it first, the event is swallowed (so
  // both need to try). Note the regression test would only fail when the
  // SetCapture() handler did _not_ swallow the event, but it still needs to try
  // to close the Widget otherwise it will be left open, which fails elsewhere.
  ClosingView* closing_view = widget->GetContentsView()->AddChildView(
      std::make_unique<ClosingView>(widget));
  widget->SetCapture(closing_view);

  ClosingEventObserver closing_event_observer(widget);
  auto monitor = EventMonitor::CreateApplicationMonitor(
      &closing_event_observer, widget->GetNativeWindow(),
      {ui::EventType::kMousePressed});

  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());
  generator->set_target(ui::test::EventGenerator::Target::APPLICATION);

  EXPECT_FALSE(observer.widget_closed());
  generator->PressLeftButton();
  EXPECT_TRUE(observer.widget_closed());
}

TEST_F(WidgetTest, LockPaintAsActive) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->ShowInactive();
  EXPECT_FALSE(widget->ShouldPaintAsActive());

  // First lock causes widget to paint as active.
  auto lock = widget->LockPaintAsActive();
  EXPECT_TRUE(widget->ShouldPaintAsActive());

  // Second lock has no effect.
  auto lock2 = widget->LockPaintAsActive();
  EXPECT_TRUE(widget->ShouldPaintAsActive());

  // Have to release twice to get back to inactive state.
  lock2.reset();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  lock.reset();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
}

TEST_F(WidgetTest, LockPaintAsActive_AlreadyActive) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->Show();
  EXPECT_TRUE(widget->ShouldPaintAsActive());

  // Lock has no effect.
  auto lock = widget->LockPaintAsActive();
  EXPECT_TRUE(widget->ShouldPaintAsActive());

  // Remove lock has no effect.
  lock.reset();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
}

TEST_F(WidgetTest, LockPaintAsActive_BecomesActive) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->ShowInactive();
  EXPECT_FALSE(widget->ShouldPaintAsActive());

  // Lock toggles render mode.
  auto lock = widget->LockPaintAsActive();
  EXPECT_TRUE(widget->ShouldPaintAsActive());

  widget->Activate();

  // Remove lock has no effect.
  lock.reset();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
}

class PaintAsActiveCallbackCounter {
 public:
  explicit PaintAsActiveCallbackCounter(Widget* widget) {
    // Subscribe to |widget|'s paint-as-active change.
    paint_as_active_subscription_ =
        widget->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
            &PaintAsActiveCallbackCounter::Call, base::Unretained(this)));
  }
  void Call() { count_++; }
  int CallCount() { return count_; }

 private:
  int count_ = 0;
  base::CallbackListSubscription paint_as_active_subscription_;
};

TEST_F(WidgetTest, LockParentPaintAsActive) {
  if (!PlatformStyle::kInactiveWidgetControlsAppearDisabled) {
    return;
  }

  WidgetAutoclosePtr parent(CreateTopLevelPlatformWidget());
  WidgetAutoclosePtr child(CreateChildPlatformWidget(parent->GetNativeView()));
  WidgetAutoclosePtr grandchild(
      CreateChildPlatformWidget(child->GetNativeView()));
  WidgetAutoclosePtr other(CreateTopLevelPlatformWidget());
  child->widget_delegate()->SetCanActivate(true);
  grandchild->widget_delegate()->SetCanActivate(true);

  PaintAsActiveCallbackCounter parent_control(parent.get());
  PaintAsActiveCallbackCounter child_control(child.get());
  PaintAsActiveCallbackCounter grandchild_control(grandchild.get());
  PaintAsActiveCallbackCounter other_control(other.get());

  parent->Show();
  EXPECT_TRUE(parent->ShouldPaintAsActive());
  EXPECT_TRUE(child->ShouldPaintAsActive());
  EXPECT_TRUE(grandchild->ShouldPaintAsActive());
  EXPECT_FALSE(other->ShouldPaintAsActive());
  EXPECT_EQ(parent_control.CallCount(), 1);
  EXPECT_EQ(child_control.CallCount(), 1);
  EXPECT_EQ(grandchild_control.CallCount(), 1);
  EXPECT_EQ(other_control.CallCount(), 0);

  other->Show();
  EXPECT_FALSE(parent->ShouldPaintAsActive());
  EXPECT_FALSE(child->ShouldPaintAsActive());
  EXPECT_FALSE(grandchild->ShouldPaintAsActive());
  EXPECT_TRUE(other->ShouldPaintAsActive());
  EXPECT_EQ(parent_control.CallCount(), 2);
  EXPECT_EQ(child_control.CallCount(), 2);
  EXPECT_EQ(grandchild_control.CallCount(), 2);
  EXPECT_EQ(other_control.CallCount(), 1);

  child->Show();
  EXPECT_TRUE(parent->ShouldPaintAsActive());
  EXPECT_TRUE(child->ShouldPaintAsActive());
  EXPECT_TRUE(grandchild->ShouldPaintAsActive());
  EXPECT_FALSE(other->ShouldPaintAsActive());
  EXPECT_EQ(parent_control.CallCount(), 3);
  EXPECT_EQ(child_control.CallCount(), 3);
  EXPECT_EQ(grandchild_control.CallCount(), 3);
  EXPECT_EQ(other_control.CallCount(), 2);

  other->Show();
  EXPECT_FALSE(parent->ShouldPaintAsActive());
  EXPECT_FALSE(child->ShouldPaintAsActive());
  EXPECT_FALSE(grandchild->ShouldPaintAsActive());
  EXPECT_TRUE(other->ShouldPaintAsActive());
  EXPECT_EQ(parent_control.CallCount(), 4);
  EXPECT_EQ(child_control.CallCount(), 4);
  EXPECT_EQ(grandchild_control.CallCount(), 4);
  EXPECT_EQ(other_control.CallCount(), 3);

  grandchild->Show();
  EXPECT_TRUE(parent->ShouldPaintAsActive());
  EXPECT_TRUE(child->ShouldPaintAsActive());
  EXPECT_TRUE(grandchild->ShouldPaintAsActive());
  EXPECT_FALSE(other->ShouldPaintAsActive());
  EXPECT_EQ(parent_control.CallCount(), 5);
  EXPECT_EQ(child_control.CallCount(), 5);
  EXPECT_EQ(grandchild_control.CallCount(), 5);
  EXPECT_EQ(other_control.CallCount(), 4);
}

// Tests to make sure that child widgets do not cause their parent widget to
// paint inactive immediately when they are closed. This avoids having the
// parent paint as inactive in the time between when the bubble is closed and
// when it's eventually destroyed by its native widget (see crbug.com/1303549).
TEST_F(DesktopWidgetTest,
       ClosingActiveChildDoesNotPrematurelyPaintParentInactive) {
  // top_level_widget that owns the bubble widget.
  auto top_level_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  top_level_widget->Show();

  // Create the child bubble widget.
  auto bubble_widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParamsForTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_BUBBLE);
  init_params.parent = top_level_widget->GetNativeView();
  bubble_widget->Init(std::move(init_params));
  bubble_widget->Show();

  EXPECT_TRUE(bubble_widget->ShouldPaintAsActive());
  EXPECT_TRUE(top_level_widget->ShouldPaintAsActive());

  // Closing the bubble wiget should not immediately cause the top level widget
  // to paint inactive.
  PaintAsActiveCallbackCounter top_level_counter(top_level_widget.get());
  PaintAsActiveCallbackCounter bubble_counter(bubble_widget.get());
  bubble_widget->Close();
  EXPECT_FALSE(bubble_widget->ShouldPaintAsActive());
  EXPECT_TRUE(top_level_widget->ShouldPaintAsActive());
  EXPECT_EQ(top_level_counter.CallCount(), 0);
  EXPECT_EQ(bubble_counter.CallCount(), 0);
}

// Tests that there is no crash when paint as active lock is removed for child
// widget while its parent widget is being closed.
TEST_F(DesktopWidgetTest, LockPaintAsActiveAndCloseParent) {
  // Make sure that DesktopNativeWidgetAura is used for widgets.
  test_views_delegate()->set_use_desktop_native_widgets(true);

  std::unique_ptr<Widget> parent =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  parent->Show();

  auto delegate = std::make_unique<TestDesktopWidgetDelegate>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.parent = parent->GetNativeView();
  delegate->InitWidget(std::move(params));
  delegate->RegisterDeleteDelegateCallback(
      base::DoNothingWithBoundArgs(delegate->GetWidget()->LockPaintAsActive()));
  base::WeakPtr<Widget> child = delegate->GetWidget()->GetWeakPtr();
  child->ShowInactive();

  // Child widget and its delegate are destroyed when the parent widget is being
  // closed. PaintAsActiveTestDesktopWidgetDelegate::paint_as_active_lock_ is
  // also deleted which should not cause a crash.
  parent->CloseNow();

  // Ensure that child widget has been destroyed.
  ASSERT_TRUE(child && child->IsClosed());
}

// Widget used to destroy itself when OnNativeWidgetDestroyed is called.
class TestNativeWidgetDestroyedWidget : public Widget {
 public:
  // Overridden from NativeWidgetDelegate:
  void OnNativeWidgetDestroyed() override;
};

void TestNativeWidgetDestroyedWidget::OnNativeWidgetDestroyed() {
  Widget::OnNativeWidgetDestroyed();
  delete this;
}

// Verifies that widget destroyed itself in OnNativeWidgetDestroyed does not
// crash in ASan.
TEST_F(DesktopWidgetTest, WidgetDestroyedItselfDoesNotCrash) {
  TestDesktopWidgetDelegate delegate(new TestNativeWidgetDestroyedWidget);
  delegate.InitWidget(
      CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  delegate.GetWidget()->Show();
  delegate.GetWidget()->CloseNow();
}

// Verifies WindowClosing() is invoked correctly on the delegate when a Widget
// is closed.
TEST_F(DesktopWidgetTest, SingleWindowClosing) {
  TestDesktopWidgetDelegate delegate;
  delegate.InitWidget(CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                   Widget::InitParams::TYPE_WINDOW));
  EXPECT_EQ(0, delegate.window_closing_count());
  delegate.GetWidget()->CloseNow();
  EXPECT_EQ(1, delegate.window_closing_count());
}

TEST_F(DesktopWidgetTest, CloseRequested_AllowsClose) {
  constexpr Widget::ClosedReason kReason = Widget::ClosedReason::kLostFocus;
  TestDesktopWidgetDelegate delegate;
  delegate.set_can_close(true);
  delegate.InitWidget(CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                   Widget::InitParams::TYPE_WINDOW));
  WidgetDestroyedWaiter waiter(delegate.GetWidget());

  delegate.GetWidget()->CloseWithReason(kReason);
  EXPECT_TRUE(delegate.GetWidget()->IsClosed());
  EXPECT_EQ(kReason, delegate.GetWidget()->closed_reason());
  EXPECT_EQ(kReason, delegate.last_closed_reason());

  waiter.Wait();
}

TEST_F(DesktopWidgetTest, CloseRequested_DisallowClose) {
  constexpr Widget::ClosedReason kReason = Widget::ClosedReason::kLostFocus;
  TestDesktopWidgetDelegate delegate;
  delegate.set_can_close(false);
  delegate.InitWidget(CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                   Widget::InitParams::TYPE_WINDOW));

  delegate.GetWidget()->CloseWithReason(kReason);
  EXPECT_FALSE(delegate.GetWidget()->IsClosed());
  EXPECT_EQ(Widget::ClosedReason::kUnspecified,
            delegate.GetWidget()->closed_reason());
  EXPECT_EQ(kReason, delegate.last_closed_reason());

  delegate.GetWidget()->CloseNow();
}

TEST_F(DesktopWidgetTest, CloseRequested_SecondCloseIgnored) {
  constexpr Widget::ClosedReason kReason1 = Widget::ClosedReason::kLostFocus;
  constexpr Widget::ClosedReason kReason2 = Widget::ClosedReason::kUnspecified;
  TestDesktopWidgetDelegate delegate;
  delegate.set_can_close(true);
  delegate.InitWidget(CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                   Widget::InitParams::TYPE_WINDOW));
  WidgetDestroyedWaiter waiter(delegate.GetWidget());

  // Close for the first time.
  delegate.GetWidget()->CloseWithReason(kReason1);
  EXPECT_TRUE(delegate.GetWidget()->IsClosed());
  EXPECT_EQ(kReason1, delegate.last_closed_reason());

  // Calling close again should have no effect.
  delegate.GetWidget()->CloseWithReason(kReason2);
  EXPECT_TRUE(delegate.GetWidget()->IsClosed());
  EXPECT_EQ(kReason1, delegate.last_closed_reason());

  waiter.Wait();
}

class WidgetWindowTitleTest : public DesktopWidgetTest {
 protected:
  void RunTest(bool desktop_native_widget) {
    auto widget = std::make_unique<Widget>();
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);

    if (!desktop_native_widget) {
      init_params.native_widget =
          CreatePlatformNativeWidgetImpl(widget.get(), kStubCapture, nullptr);
    }
    widget->Init(std::move(init_params));

    internal::NativeWidgetPrivate* native_widget =
        widget->native_widget_private();

    std::u16string empty;
    std::u16string s1(u"Title1");
    std::u16string s2(u"Title2");
    std::u16string s3(u"TitleLong");

    // The widget starts with no title, setting empty should not change
    // anything.
    EXPECT_FALSE(native_widget->SetWindowTitle(empty));
    // Setting the title to something non-empty should cause a change.
    EXPECT_TRUE(native_widget->SetWindowTitle(s1));
    // Setting the title to something else with the same length should cause a
    // change.
    EXPECT_TRUE(native_widget->SetWindowTitle(s2));
    // Setting the title to something else with a different length should cause
    // a change.
    EXPECT_TRUE(native_widget->SetWindowTitle(s3));
    // Setting the title to the same thing twice should not cause a change.
    EXPECT_FALSE(native_widget->SetWindowTitle(s3));
  }
};

TEST_F(WidgetWindowTitleTest, SetWindowTitleChanged_NativeWidget) {
  // Use the default NativeWidget.
  bool desktop_native_widget = false;
  RunTest(desktop_native_widget);
}

TEST_F(WidgetWindowTitleTest, SetWindowTitleChanged_DesktopNativeWidget) {
  // Override to use a DesktopNativeWidget.
  bool desktop_native_widget = true;
  RunTest(desktop_native_widget);
}

TEST_F(WidgetTest, WidgetDeleted_InOnMousePressed) {
  Widget* widget = new Widget;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));

  widget->SetContentsView(
      std::make_unique<CloseWidgetView>(ui::EventType::kMousePressed));

  widget->SetSize(gfx::Size(100, 100));
  widget->Show();

  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());

  WidgetDeletionObserver deletion_observer(widget);
  generator->PressLeftButton();
  if (deletion_observer.IsWidgetAlive()) {
    generator->ReleaseLeftButton();
  }
  EXPECT_FALSE(deletion_observer.IsWidgetAlive());

  // Yay we did not crash!
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)

TEST_F(WidgetTest, WidgetDeleted_InDispatchGestureEvent) {
  Widget* widget = new Widget;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));

  widget->SetContentsView(
      std::make_unique<CloseWidgetView>(ui::EventType::kGestureTapDown));

  widget->SetSize(gfx::Size(100, 100));
  widget->Show();

  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());

  WidgetDeletionObserver deletion_observer(widget);
  generator->GestureTapAt(widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_FALSE(deletion_observer.IsWidgetAlive());

  // Yay we did not crash!
}

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)

// See description of RunGetNativeThemeFromDestructor() for details.
class GetNativeThemeFromDestructorView : public WidgetDelegateView {
 public:
  GetNativeThemeFromDestructorView() = default;

  GetNativeThemeFromDestructorView(const GetNativeThemeFromDestructorView&) =
      delete;
  GetNativeThemeFromDestructorView& operator=(
      const GetNativeThemeFromDestructorView&) = delete;

  ~GetNativeThemeFromDestructorView() override { VerifyNativeTheme(); }

 private:
  void VerifyNativeTheme() { ASSERT_TRUE(GetNativeTheme() != nullptr); }
};

// Verifies GetNativeTheme() from the destructor of a WidgetDelegateView doesn't
// crash. |is_first_run| is true if this is the first call. A return value of
// true indicates this should be run again with a value of false.
// First run uses DesktopNativeWidgetAura (if possible). Second run doesn't.
bool RunGetNativeThemeFromDestructor(Widget::InitParams params,
                                     bool is_first_run) {
  bool needs_second_run = false;
  // Destroyed by CloseNow() below.
  WidgetTest::WidgetAutoclosePtr widget(new Widget);
  // Deletes itself when the Widget is destroyed.
  params.delegate = new GetNativeThemeFromDestructorView;
  if (!is_first_run) {
    params.native_widget =
        CreatePlatformNativeWidgetImpl(widget.get(), kStubCapture, nullptr);
    needs_second_run = true;
  }
  widget->Init(std::move(params));
  return needs_second_run;
}

// See description of RunGetNativeThemeFromDestructor() for details.
TEST_F(DesktopWidgetTest, DISABLED_GetNativeThemeFromDestructor) {
  if (RunGetNativeThemeFromDestructor(
          CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                       Widget::InitParams::TYPE_POPUP),
          true)) {
    RunGetNativeThemeFromDestructor(
        CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                     Widget::InitParams::TYPE_POPUP),
        false);
  }
}

// Used by HideCloseDestroy. Allows setting a boolean when the widget is
// destroyed.
class CloseDestroysWidget : public Widget {
 public:
  CloseDestroysWidget(bool* destroyed, base::OnceClosure quit_closure)
      : destroyed_(destroyed), quit_closure_(std::move(quit_closure)) {
    DCHECK(destroyed_);
    DCHECK(quit_closure_);
  }

  CloseDestroysWidget(const CloseDestroysWidget&) = delete;
  CloseDestroysWidget& operator=(const CloseDestroysWidget&) = delete;

  ~CloseDestroysWidget() override {
    *destroyed_ = true;
    std::move(quit_closure_).Run();
  }

  void Detach() { destroyed_ = nullptr; }

 private:
  raw_ptr<bool> destroyed_;
  base::OnceClosure quit_closure_;
};

// An observer that registers that an animation has ended.
class AnimationEndObserver : public ui::ImplicitAnimationObserver {
 public:
  AnimationEndObserver() = default;

  AnimationEndObserver(const AnimationEndObserver&) = delete;
  AnimationEndObserver& operator=(const AnimationEndObserver&) = delete;

  ~AnimationEndObserver() override = default;

  bool animation_completed() const { return animation_completed_; }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { animation_completed_ = true; }

 private:
  bool animation_completed_ = false;
};

// An observer that registers the bounds of a widget on destruction.
class WidgetBoundsObserver : public WidgetObserver {
 public:
  explicit WidgetBoundsObserver(Widget* widget) {
    widget_observation_.Observe(widget);
  }

  WidgetBoundsObserver(const WidgetBoundsObserver&) = delete;
  WidgetBoundsObserver& operator=(const WidgetBoundsObserver&) = delete;

  ~WidgetBoundsObserver() override = default;

  gfx::Rect bounds() { return bounds_; }

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override {
    EXPECT_TRUE(widget->GetNativeWindow());
    EXPECT_TRUE(Widget::GetWidgetForNativeWindow(widget->GetNativeWindow()));
    bounds_ = widget->GetWindowBoundsInScreen();
    widget_observation_.Reset();
  }

 private:
  gfx::Rect bounds_;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

// Verifies Close() results in destroying.
TEST_F(DesktopWidgetTest, CloseDestroys) {
  bool destroyed = false;
  base::RunLoop run_loop;
  CloseDestroysWidget* widget =
      new CloseDestroysWidget(&destroyed, run_loop.QuitClosure());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   Widget::InitParams::TYPE_MENU);
  params.opacity = Widget::InitParams::WindowOpacity::kOpaque;
  params.bounds = gfx::Rect(50, 50, 250, 250);
  widget->Init(std::move(params));
  widget->Show();
  widget->Hide();
  widget->Close();
  EXPECT_FALSE(destroyed);
  // Run the message loop as Close() asynchronously deletes.
  run_loop.Run();
  EXPECT_TRUE(destroyed);
  // Close() should destroy the widget. If not we'll cleanup to avoid leaks.
  if (!destroyed) {
    widget->Detach();
    widget->CloseNow();
  }
}

// Tests that killing a widget while animating it does not crash.
TEST_F(WidgetTest, CloseWidgetWhileAnimating) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  AnimationEndObserver animation_observer;
  WidgetBoundsObserver widget_observer(widget.get());
  gfx::Rect bounds(100, 100, 50, 50);
  {
    // Normal animations for tests have ZERO_DURATION, make sure we are actually
    // animating the movement.
    ui::ScopedAnimationDurationScaleMode animation_scale_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    ui::ScopedLayerAnimationSettings animation_settings(
        widget->GetLayer()->GetAnimator());
    animation_settings.AddObserver(&animation_observer);
    widget->Show();

    // Animate the bounds change.
    widget->SetBounds(bounds);
    widget->CloseNow();
    widget.reset();
    EXPECT_FALSE(animation_observer.animation_completed());
  }
  EXPECT_TRUE(animation_observer.animation_completed());
  EXPECT_EQ(widget_observer.bounds(), bounds);
}

// Test Widget::CloseAllSecondaryWidgets works as expected across platforms.
// ChromeOS doesn't implement or need CloseAllSecondaryWidgets() since
// everything is under a single root window.
#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)
TEST_F(DesktopWidgetTest, CloseAllSecondaryWidgets) {
  Widget* widget1 = CreateTopLevelNativeWidget();
  Widget* widget2 = CreateTopLevelNativeWidget();
  TestWidgetObserver observer1(widget1);
  TestWidgetObserver observer2(widget2);
  widget1->Show();  // Just show the first one.
  Widget::CloseAllSecondaryWidgets();
  EXPECT_TRUE(observer1.widget_closed());
  EXPECT_TRUE(observer2.widget_closed());
}
#endif

// Test that the NativeWidget is still valid during OnNativeWidgetDestroying(),
// and properties that depend on it are valid, when closed via CloseNow().
TEST_F(DesktopWidgetTest, ValidDuringOnNativeWidgetDestroyingFromCloseNow) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->Show();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);
  WidgetBoundsObserver observer(widget);
  widget->CloseNow();
  EXPECT_EQ(screen_rect, observer.bounds());
}

// Test that the NativeWidget is still valid during OnNativeWidgetDestroying(),
// and properties that depend on it are valid, when closed via Close().
TEST_F(DesktopWidgetTest, ValidDuringOnNativeWidgetDestroyingFromClose) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->Show();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);
  WidgetBoundsObserver observer(widget);
  widget->Close();
  EXPECT_EQ(gfx::Rect(), observer.bounds());
  base::RunLoop().RunUntilIdle();
// Broken on Linux. See http://crbug.com/515379.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  EXPECT_EQ(screen_rect, observer.bounds());
#endif
}

// Tests that we do not crash when a Widget is destroyed by going out of
// scope (as opposed to being explicitly deleted by its NativeWidget).
TEST_F(WidgetTest, NoCrashOnWidgetDelete) {
  CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
}

TEST_F(WidgetTest, NoCrashOnResizeConstraintsWindowTitleOnPopup) {
  CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_POPUP)
      ->OnSizeConstraintsChanged();
}

// Tests that we do not crash when a Widget is destroyed before it finishes
// processing of pending input events in the message loop.
TEST_F(WidgetTest, NoCrashOnWidgetDeleteWithPendingEvents) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();

  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());
  generator->MoveMouseTo(10, 10);

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if BUILDFLAG(IS_MAC)
  generator->ClickLeftButton();
#else
  generator->PressTouch();
#endif
  widget.reset();
}

// A view that consumes mouse-pressed event and gesture-tap-down events.
class RootViewTestView : public View {
  METADATA_HEADER(RootViewTestView, View)

 public:
  RootViewTestView() = default;

 private:
  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::EventType::kGestureTapDown) {
      event->SetHandled();
    }
  }
};

BEGIN_METADATA(RootViewTestView)
END_METADATA

// Checks if RootView::*_handler_ fields are unset when widget is hidden.
// Fails on chromium.webkit Windows bot, see crbug.com/264872.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DisableTestRootViewHandlersWhenHidden \
  DISABLED_TestRootViewHandlersWhenHidden
#else
#define MAYBE_DisableTestRootViewHandlersWhenHidden \
  TestRootViewHandlersWhenHidden
#endif
TEST_F(WidgetTest, MAYBE_DisableTestRootViewHandlersWhenHidden) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  View* view = new RootViewTestView();
  view->SetBounds(0, 0, 300, 300);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  root_view->AddChildView(view);

  // Check RootView::mouse_pressed_handler_.
  widget->Show();
  EXPECT_EQ(nullptr, GetMousePressedHandler(root_view));
  gfx::Point click_location(45, 15);
  ui::MouseEvent press(ui::EventType::kMousePressed, click_location,
                       click_location, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  widget->OnMouseEvent(&press);
  EXPECT_EQ(view, GetMousePressedHandler(root_view));
  widget->Hide();
  EXPECT_EQ(nullptr, GetMousePressedHandler(root_view));

  // Check RootView::mouse_move_handler_.
  widget->Show();
  EXPECT_EQ(nullptr, GetMouseMoveHandler(root_view));
  gfx::Point move_location(45, 15);
  ui::MouseEvent move(ui::EventType::kMouseMoved, move_location, move_location,
                      ui::EventTimeForNow(), 0, 0);
  widget->OnMouseEvent(&move);
  EXPECT_EQ(view, GetMouseMoveHandler(root_view));
  widget->Hide();
  EXPECT_EQ(nullptr, GetMouseMoveHandler(root_view));

  // Check RootView::gesture_handler_.
  widget->Show();
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  ui::GestureEvent tap_down =
      CreateTestGestureEvent(ui::EventType::kGestureTapDown, 15, 15);
  widget->OnGestureEvent(&tap_down);
  EXPECT_EQ(view, GetGestureHandler(root_view));
  widget->Hide();
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));

  widget->Close();
}

// Tests that the |gesture_handler_| member in RootView is always NULL
// after the dispatch of a ui::EventType::kGestureEnd event corresponding to
// the release of the final touch point on the screen, but that
// ui::EventType::kGestureEnd events corresponding to the removal of any other
// touch point do not modify |gesture_handler_|.
TEST_F(WidgetTest, GestureEndEvents) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  EventCountView* view = new EventCountView();
  view->SetBounds(0, 0, 300, 300);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  root_view->AddChildView(view);
  widget->Show();

  // If no gesture handler is set, a ui::EventType::kGestureEnd event should not
  // set the gesture handler.
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  ui::GestureEvent end =
      CreateTestGestureEvent(ui::EventType::kGestureEnd, 15, 15);
  widget->OnGestureEvent(&end);
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));

  // Change the handle mode of |view| to indicate that it would like
  // to handle all events, then send a GESTURE_TAP to set the gesture handler.
  view->set_handle_mode(EventCountView::CONSUME_EVENTS);
  ui::GestureEvent tap =
      CreateTestGestureEvent(ui::EventType::kGestureTap, 15, 15);
  widget->OnGestureEvent(&tap);
  EXPECT_TRUE(tap.handled());
  EXPECT_EQ(view, GetGestureHandler(root_view));

  // The gesture handler should remain unchanged on a ui::EventType::kGestureEnd
  // corresponding to a second touch point, but should be reset to NULL by a
  // ui::EventType::kGestureEnd corresponding to the final touch point.
  ui::GestureEventDetails details(ui::EventType::kGestureEnd);
  details.set_touch_points(2);
  ui::GestureEvent end_second_touch_point =
      CreateTestGestureEvent(details, 15, 15);
  widget->OnGestureEvent(&end_second_touch_point);
  EXPECT_EQ(view, GetGestureHandler(root_view));

  end = CreateTestGestureEvent(ui::EventType::kGestureEnd, 15, 15);
  widget->OnGestureEvent(&end);
  EXPECT_TRUE(end.handled());
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));

  // Send a GESTURE_TAP to set the gesture handler, then change the handle
  // mode of |view| to indicate that it does not want to handle any
  // further events.
  tap = CreateTestGestureEvent(ui::EventType::kGestureTap, 15, 15);
  widget->OnGestureEvent(&tap);
  EXPECT_TRUE(tap.handled());
  EXPECT_EQ(view, GetGestureHandler(root_view));
  view->set_handle_mode(EventCountView::PROPAGATE_EVENTS);

  // The gesture handler should remain unchanged on a ui::EventType::kGestureEnd
  // corresponding to a second touch point, but should be reset to NULL by a
  // ui::EventType::kGestureEnd corresponding to the final touch point.
  end_second_touch_point = CreateTestGestureEvent(details, 15, 15);
  widget->OnGestureEvent(&end_second_touch_point);
  EXPECT_EQ(view, GetGestureHandler(root_view));

  end = CreateTestGestureEvent(ui::EventType::kGestureEnd, 15, 15);
  widget->OnGestureEvent(&end);
  EXPECT_FALSE(end.handled());
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));

  widget->Close();
}

// Tests that gesture events which should not be processed (because
// RootView::OnEventProcessingStarted() has marked them as handled) are not
// dispatched to any views.
TEST_F(WidgetTest, GestureEventsNotProcessed) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));

  // Define a hierarchy of four views (coordinates are in
  // their parent coordinate space).
  // v1 (0, 0, 300, 300)
  //   v2 (0, 0, 100, 100)
  //     v3 (0, 0, 50, 50)
  //       v4(0, 0, 10, 10)
  EventCountView* v1 = new EventCountView();
  v1->SetBounds(0, 0, 300, 300);
  EventCountView* v2 = new EventCountView();
  v2->SetBounds(0, 0, 100, 100);
  EventCountView* v3 = new EventCountView();
  v3->SetBounds(0, 0, 50, 50);
  EventCountView* v4 = new EventCountView();
  v4->SetBounds(0, 0, 10, 10);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  root_view->AddChildView(v1);
  v1->AddChildView(v2);
  v2->AddChildView(v3);
  v3->AddChildView(v4);

  widget->Show();

  // ui::EventType::kGestureBegin events should never be seen by any view, but
  // they should be marked as handled by OnEventProcessingStarted().
  ui::GestureEvent begin =
      CreateTestGestureEvent(ui::EventType::kGestureBegin, 5, 5);
  widget->OnGestureEvent(&begin);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureBegin));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureBegin));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGestureBegin));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureBegin));
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  EXPECT_TRUE(begin.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // ui::EventType::kGestureEnd events should not be seen by any view when there
  // is no default gesture handler set, but they should be marked as handled by
  // OnEventProcessingStarted().
  ui::GestureEvent end =
      CreateTestGestureEvent(ui::EventType::kGestureEnd, 5, 5);
  widget->OnGestureEvent(&end);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  EXPECT_TRUE(end.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // ui::EventType::kGestureEnd events not corresponding to the release of the
  // final touch point should never be seen by any view, but they should
  // be marked as handled by OnEventProcessingStarted().
  ui::GestureEventDetails details(ui::EventType::kGestureEnd);
  details.set_touch_points(2);
  ui::GestureEvent end_second_touch_point =
      CreateTestGestureEvent(details, 5, 5);
  widget->OnGestureEvent(&end_second_touch_point);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  EXPECT_TRUE(end_second_touch_point.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // ui::EventType::kGestureScrollUpdate events should never be seen by any view
  // when there is no default gesture handler set, but they should be marked as
  // handled by OnEventProcessingStarted().
  ui::GestureEvent scroll_update =
      CreateTestGestureEvent(ui::EventType::kGestureScrollUpdate, 5, 5);
  widget->OnGestureEvent(&scroll_update);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureScrollUpdate));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureScrollUpdate));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGestureScrollUpdate));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureScrollUpdate));
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  EXPECT_TRUE(scroll_update.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // ui::EventType::kGestureScrollEnd events should never be seen by any view
  // when there is no default gesture handler set, but they should be marked as
  // handled by OnEventProcessingStarted().
  ui::GestureEvent scroll_end =
      CreateTestGestureEvent(ui::EventType::kGestureScrollEnd, 5, 5);
  widget->OnGestureEvent(&scroll_end);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  EXPECT_TRUE(scroll_end.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // ui::EventType::kScrollFlingStart events should never be seen by any view
  // when there is no default gesture handler set, but they should be marked as
  // handled by OnEventProcessingStarted().
  ui::GestureEvent scroll_fling_start =
      CreateTestGestureEvent(ui::EventType::kScrollFlingStart, 5, 5);
  widget->OnGestureEvent(&scroll_fling_start);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kScrollFlingStart));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kScrollFlingStart));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kScrollFlingStart));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kScrollFlingStart));
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  EXPECT_TRUE(scroll_fling_start.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  widget->Close();
}

// Tests that a (non-scroll) gesture event is dispatched to the correct views
// in a view hierarchy and that the default gesture handler in RootView is set
// correctly.
TEST_F(WidgetTest, GestureEventDispatch) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));

  // Define a hierarchy of four views (coordinates are in
  // their parent coordinate space).
  // v1 (0, 0, 300, 300)
  //   v2 (0, 0, 100, 100)
  //     v3 (0, 0, 50, 50)
  //       v4(0, 0, 10, 10)
  EventCountView* v1 = new EventCountView();
  v1->SetBounds(0, 0, 300, 300);
  EventCountView* v2 = new EventCountView();
  v2->SetBounds(0, 0, 100, 100);
  EventCountView* v3 = new EventCountView();
  v3->SetBounds(0, 0, 50, 50);
  EventCountView* v4 = new EventCountView();
  v4->SetBounds(0, 0, 10, 10);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  root_view->AddChildView(v1);
  v1->AddChildView(v2);
  v2->AddChildView(v3);
  v3->AddChildView(v4);

  widget->Show();

  // No gesture handler is set in the root view and none of the views in the
  // view hierarchy handle a ui::EventType::kGestureTap event. In this case the
  // tap event should be dispatched to all views in the hierarchy, the gesture
  // handler should remain unset, and the event should remain unhandled.
  ui::GestureEvent tap =
      CreateTestGestureEvent(ui::EventType::kGestureTap, 5, 5);
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  widget->OnGestureEvent(&tap);
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(1, v2->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(1, v4->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  EXPECT_FALSE(tap.handled());

  // No gesture handler is set in the root view and |v1|, |v2|, and |v3| all
  // handle a ui::EventType::kGestureTap event. In this case the tap event
  // should be dispatched to |v4| and |v3|, the gesture handler should be set to
  // |v3|, and the event should be marked as handled.
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();
  v1->set_handle_mode(EventCountView::CONSUME_EVENTS);
  v2->set_handle_mode(EventCountView::CONSUME_EVENTS);
  v3->set_handle_mode(EventCountView::CONSUME_EVENTS);
  tap = CreateTestGestureEvent(ui::EventType::kGestureTap, 5, 5);
  widget->OnGestureEvent(&tap);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(1, v4->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(v3, GetGestureHandler(root_view));
  EXPECT_TRUE(tap.handled());

  // The gesture handler is set to |v3| and all views handle all gesture event
  // types. In this case subsequent gesture events should only be dispatched to
  // |v3| and marked as handled. The gesture handler should remain as |v3|.
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();
  v4->set_handle_mode(EventCountView::CONSUME_EVENTS);
  tap = CreateTestGestureEvent(ui::EventType::kGestureTap, 5, 5);
  widget->OnGestureEvent(&tap);
  EXPECT_TRUE(tap.handled());
  ui::GestureEvent show_press =
      CreateTestGestureEvent(ui::EventType::kGestureShowPress, 5, 5);
  widget->OnGestureEvent(&show_press);
  tap = CreateTestGestureEvent(ui::EventType::kGestureTap, 5, 5);
  widget->OnGestureEvent(&tap);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(2, v3->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureShowPress));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureShowPress));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kGestureShowPress));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureShowPress));
  EXPECT_TRUE(tap.handled());
  EXPECT_TRUE(show_press.handled());
  EXPECT_EQ(v3, GetGestureHandler(root_view));

  // The gesture handler is set to |v3|, but |v3| does not handle
  // ui::EventType::kGestureTap events. In this case a tap gesture should be
  // dispatched only to |v3|, but the event should remain unhandled. The gesture
  // handler should remain as |v3|.
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();
  v3->set_handle_mode(EventCountView::PROPAGATE_EVENTS);
  tap = CreateTestGestureEvent(ui::EventType::kGestureTap, 5, 5);
  widget->OnGestureEvent(&tap);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_FALSE(tap.handled());
  EXPECT_EQ(v3, GetGestureHandler(root_view));

  widget->Close();
}

// Tests that gesture scroll events will change the default gesture handler in
// RootView if the current handler to which they are dispatched does not handle
// gesture scroll events.
TEST_F(WidgetTest, ScrollGestureEventDispatch) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));

  // Define a hierarchy of four views (coordinates are in
  // their parent coordinate space).
  // v1 (0, 0, 300, 300)
  //   v2 (0, 0, 100, 100)
  //     v3 (0, 0, 50, 50)
  //       v4(0, 0, 10, 10)
  EventCountView* v1 = new EventCountView();
  v1->SetBounds(0, 0, 300, 300);
  EventCountView* v2 = new EventCountView();
  v2->SetBounds(0, 0, 100, 100);
  EventCountView* v3 = new EventCountView();
  v3->SetBounds(0, 0, 50, 50);
  EventCountView* v4 = new EventCountView();
  v4->SetBounds(0, 0, 10, 10);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  root_view->AddChildView(v1);
  v1->AddChildView(v2);
  v2->AddChildView(v3);
  v3->AddChildView(v4);

  widget->Show();

  // Change the handle mode of |v3| to indicate that it would like to handle
  // gesture events.
  v3->set_handle_mode(EventCountView::CONSUME_EVENTS);

  // When no gesture handler is set, dispatching a
  // ui::EventType::kGestureTapDown should bubble up the views hierarchy until
  // it reaches the first view that will handle it (|v3|) and then sets the
  // handler to |v3|.
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  ui::GestureEvent tap_down =
      CreateTestGestureEvent(ui::EventType::kGestureTapDown, 5, 5);
  widget->OnGestureEvent(&tap_down);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureTapDown));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureTapDown));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kGestureTapDown));
  EXPECT_EQ(1, v4->GetEventCount(ui::EventType::kGestureTapDown));
  EXPECT_EQ(v3, GetGestureHandler(root_view));
  EXPECT_TRUE(tap_down.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // A ui::EventType::kGestureTapCancel event should be dispatched to |v3|
  // directly.
  ui::GestureEvent tap_cancel =
      CreateTestGestureEvent(ui::EventType::kGestureTapCancel, 5, 5);
  widget->OnGestureEvent(&tap_cancel);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kGestureTapCancel));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureTapCancel));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kGestureTapCancel));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureTapCancel));
  EXPECT_EQ(v3, GetGestureHandler(root_view));
  EXPECT_TRUE(tap_cancel.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // Change the handle mode of |v3| to indicate that it would no longer like
  // to handle events, and change the mode of |v1| to indicate that it would
  // like to handle events.
  v3->set_handle_mode(EventCountView::PROPAGATE_EVENTS);
  v1->set_handle_mode(EventCountView::CONSUME_EVENTS);

  // Dispatch a ui::EventType::kGestureScrollBegin event. Because the current
  // gesture handler (|v3|) does not handle scroll events, the event should
  // bubble up the views hierarchy until it reaches the first view that will
  // handle it (|v1|) and then sets the handler to |v1|.
  ui::GestureEvent scroll_begin =
      CreateTestGestureEvent(ui::EventType::kGestureScrollBegin, 5, 5);
  widget->OnGestureEvent(&scroll_begin);
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kGestureScrollBegin));
  EXPECT_EQ(1, v2->GetEventCount(ui::EventType::kGestureScrollBegin));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kGestureScrollBegin));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureScrollBegin));
  EXPECT_EQ(v1, GetGestureHandler(root_view));
  EXPECT_TRUE(scroll_begin.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // A ui::EventType::kGestureScrollUpdate event should be dispatched to |v1|
  // directly.
  ui::GestureEvent scroll_update =
      CreateTestGestureEvent(ui::EventType::kGestureScrollUpdate, 5, 5);
  widget->OnGestureEvent(&scroll_update);
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kGestureScrollUpdate));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureScrollUpdate));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGestureScrollUpdate));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureScrollUpdate));
  EXPECT_EQ(v1, GetGestureHandler(root_view));
  EXPECT_TRUE(scroll_update.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // A ui::EventType::kGestureScrollEnd event should be dispatched to |v1|
  // directly and should not reset the gesture handler.
  ui::GestureEvent scroll_end =
      CreateTestGestureEvent(ui::EventType::kGestureScrollEnd, 5, 5);
  widget->OnGestureEvent(&scroll_end);
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(v1, GetGestureHandler(root_view));
  EXPECT_TRUE(scroll_end.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // A ui::EventType::kGesturePinchBegin event (which is a non-scroll event)
  // should still be dispatched to |v1| directly.
  ui::GestureEvent pinch_begin =
      CreateTestGestureEvent(ui::EventType::kGesturePinchBegin, 5, 5);
  widget->OnGestureEvent(&pinch_begin);
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kGesturePinchBegin));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGesturePinchBegin));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGesturePinchBegin));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGesturePinchBegin));
  EXPECT_EQ(v1, GetGestureHandler(root_view));
  EXPECT_TRUE(pinch_begin.handled());
  v1->ResetCounts();
  v2->ResetCounts();
  v3->ResetCounts();
  v4->ResetCounts();

  // A ui::EventType::kGestureEnd event should be dispatched to |v1| and should
  // set the gesture handler to NULL.
  ui::GestureEvent end =
      CreateTestGestureEvent(ui::EventType::kGestureEnd, 5, 5);
  widget->OnGestureEvent(&end);
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v3->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(0, v4->GetEventCount(ui::EventType::kGestureEnd));
  EXPECT_EQ(nullptr, GetGestureHandler(root_view));
  EXPECT_TRUE(end.handled());

  widget->Close();
}

// TODO(b/271490637): on Mac a drag controller should still be notified when
// drag will start. Figure out how to write a unit test for Mac. Then remove
// this build flag check.
#if !BUILDFLAG(IS_MAC)

// Verifies that the drag controller is notified when the view drag will start.
TEST_F(WidgetTest, NotifyDragControllerWhenDragWillStart) {
  // Create a widget whose contents view is draggable.
  UniqueWidgetPtr widget(std::make_unique<Widget>());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(/*width=*/650, /*height=*/650);
  widget->Init(std::move(params));
  widget->Show();
  MockDragController mock_drag_controller;
  views::View contents_view;
  contents_view.set_drag_controller(&mock_drag_controller);
  widget->SetContentsView(&contents_view);

  // Expect the drag controller is notified of the drag start.
  EXPECT_CALL(mock_drag_controller, OnWillStartDragForView(&contents_view));

  // Drag-and-drop `contents_view` by mouse.
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  generator.MoveMouseTo(contents_view.GetBoundsInScreen().CenterPoint());
  generator.PressLeftButton();
  generator.MoveMouseBy(/*x=*/200, /*y=*/0);
  generator.ReleaseLeftButton();
}

#endif  // !BUILDFLAG(IS_MAC)

// A class used in WidgetTest.GestureEventLocationWhileBubbling to verify
// that when a gesture event bubbles up a View hierarchy, the location
// of a gesture event seen by each View is in the local coordinate space
// of that View.
class GestureLocationView : public EventCountView {
  METADATA_HEADER(GestureLocationView, EventCountView)

 public:
  GestureLocationView() = default;

  GestureLocationView(const GestureLocationView&) = delete;
  GestureLocationView& operator=(const GestureLocationView&) = delete;

  ~GestureLocationView() override = default;

  void set_expected_location(gfx::Point expected_location) {
    expected_location_ = expected_location;
  }

  // EventCountView:
  void OnGestureEvent(ui::GestureEvent* event) override {
    EventCountView::OnGestureEvent(event);

    // Verify that the location of |event| is in the local coordinate
    // space of |this|.
    EXPECT_EQ(expected_location_, event->location());
  }

 private:
  // The expected location of a gesture event dispatched to |this|.
  gfx::Point expected_location_;
};

BEGIN_METADATA(GestureLocationView)
END_METADATA

// Verifies that the location of a gesture event is always in the local
// coordinate space of the View receiving the event while bubbling.
TEST_F(WidgetTest, GestureEventLocationWhileBubbling) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));

  // Define a hierarchy of three views (coordinates shown below are in the
  // coordinate space of the root view, but the coordinates used for
  // SetBounds() are in their parent coordinate space).
  // v1 (50, 50, 150, 150)
  //   v2 (100, 70, 50, 80)
  //     v3 (120, 100, 10, 10)
  GestureLocationView* v1 = new GestureLocationView();
  v1->SetBounds(50, 50, 150, 150);
  GestureLocationView* v2 = new GestureLocationView();
  v2->SetBounds(50, 20, 50, 80);
  GestureLocationView* v3 = new GestureLocationView();
  v3->SetBounds(20, 30, 10, 10);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  root_view->AddChildView(v1);
  v1->AddChildView(v2);
  v2->AddChildView(v3);

  widget->Show();

  // Define a GESTURE_TAP event located at (125, 105) in root view coordinates.
  // This event is contained within all of |v1|, |v2|, and |v3|.
  gfx::Point location_in_root(125, 105);
  ui::GestureEvent tap = CreateTestGestureEvent(
      ui::EventType::kGestureTap, location_in_root.x(), location_in_root.y());

  // Calculate the location of the event in the local coordinate spaces
  // of each of the views.
  gfx::Point location_in_v1(ConvertPointFromWidgetToView(v1, location_in_root));
  EXPECT_EQ(gfx::Point(75, 55), location_in_v1);
  gfx::Point location_in_v2(ConvertPointFromWidgetToView(v2, location_in_root));
  EXPECT_EQ(gfx::Point(25, 35), location_in_v2);
  gfx::Point location_in_v3(ConvertPointFromWidgetToView(v3, location_in_root));
  EXPECT_EQ(gfx::Point(5, 5), location_in_v3);

  // Dispatch the event. When each view receives the event, its location should
  // be in the local coordinate space of that view (see the check made by
  // GestureLocationView). After dispatch is complete the event's location
  // should be in the root coordinate space.
  v1->set_expected_location(location_in_v1);
  v2->set_expected_location(location_in_v2);
  v3->set_expected_location(location_in_v3);
  widget->OnGestureEvent(&tap);
  EXPECT_EQ(location_in_root, tap.location());

  // Verify that each view did in fact see the event.
  EventCountView* view1 = v1;
  EventCountView* view2 = v2;
  EventCountView* view3 = v3;
  EXPECT_EQ(1, view1->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(1, view2->GetEventCount(ui::EventType::kGestureTap));
  EXPECT_EQ(1, view3->GetEventCount(ui::EventType::kGestureTap));

  widget->Close();
}

// Test the result of Widget::GetAllChildWidgets().
TEST_F(WidgetTest, GetAllChildWidgets) {
  // Create the following widget hierarchy:
  //
  // toplevel
  // +-- w1
  //     +-- w11
  // +-- w2
  //     +-- w21
  //     +-- w22
  WidgetAutoclosePtr toplevel(CreateTopLevelPlatformWidget());
  Widget* w1 = CreateChildPlatformWidget(toplevel->GetNativeView());
  Widget* w11 = CreateChildPlatformWidget(w1->GetNativeView());
  Widget* w2 = CreateChildPlatformWidget(toplevel->GetNativeView());
  Widget* w21 = CreateChildPlatformWidget(w2->GetNativeView());
  Widget* w22 = CreateChildPlatformWidget(w2->GetNativeView());

  std::set<Widget*> expected;
  expected.insert(toplevel.get());
  expected.insert(w1);
  expected.insert(w11);
  expected.insert(w2);
  expected.insert(w21);
  expected.insert(w22);

  std::set<raw_ptr<Widget, SetExperimental>> child_widgets;
  Widget::GetAllChildWidgets(toplevel->GetNativeView(), &child_widgets);

  EXPECT_TRUE(base::ranges::equal(expected, child_widgets));

  // Check GetAllOwnedWidgets(). On Aura, this includes "transient" children.
  // Otherwise (on all platforms), it should be the same as GetAllChildWidgets()
  // except the root Widget is not included.
  EXPECT_EQ(1u, expected.erase(toplevel.get()));

  std::set<raw_ptr<Widget, SetExperimental>> owned_widgets;
  Widget::GetAllOwnedWidgets(toplevel->GetNativeView(), &owned_widgets);

  EXPECT_TRUE(base::ranges::equal(expected, owned_widgets));
}

// Used by DestroyChildWidgetsInOrder. On destruction adds the supplied name to
// a vector.
class DestroyedTrackingView : public View {
  METADATA_HEADER(DestroyedTrackingView, View)

 public:
  DestroyedTrackingView(const std::string& name,
                        std::vector<std::string>* add_to)
      : name_(name), add_to_(add_to) {}

  DestroyedTrackingView(const DestroyedTrackingView&) = delete;
  DestroyedTrackingView& operator=(const DestroyedTrackingView&) = delete;

  ~DestroyedTrackingView() override { add_to_->push_back(name_); }

 private:
  const std::string name_;
  raw_ptr<std::vector<std::string>> add_to_;
};

BEGIN_METADATA(DestroyedTrackingView)
END_METADATA

class WidgetChildDestructionTest : public DesktopWidgetTest {
 public:
  WidgetChildDestructionTest() = default;

  WidgetChildDestructionTest(const WidgetChildDestructionTest&) = delete;
  WidgetChildDestructionTest& operator=(const WidgetChildDestructionTest&) =
      delete;

  // Creates a top level and a child, destroys the child and verifies the views
  // of the child are destroyed before the views of the parent.
  void RunDestroyChildWidgetsTest(bool top_level_has_desktop_native_widget_aura,
                                  bool child_has_desktop_native_widget_aura) {
    // When a View is destroyed its name is added here.
    std::vector<std::string> destroyed;

    Widget* top_level = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    if (!top_level_has_desktop_native_widget_aura) {
      params.native_widget =
          CreatePlatformNativeWidgetImpl(top_level, kStubCapture, nullptr);
    }
    top_level->Init(std::move(params));
    top_level->GetRootView()->AddChildView(
        new DestroyedTrackingView("parent", &destroyed));
    top_level->Show();

    Widget* child = new Widget;
    Widget::InitParams child_params =
        CreateParams(Widget::InitParams::TYPE_POPUP);
    child_params.parent = top_level->GetNativeView();
    if (!child_has_desktop_native_widget_aura) {
      child_params.native_widget =
          CreatePlatformNativeWidgetImpl(child, kStubCapture, nullptr);
    }
    child->Init(std::move(child_params));
    child->GetRootView()->AddChildView(
        new DestroyedTrackingView("child", &destroyed));
    child->Show();

    // Should trigger destruction of the child too.
    top_level->native_widget_private()->CloseNow();

    // Child should be destroyed first.
    ASSERT_EQ(2u, destroyed.size());
    EXPECT_EQ("child", destroyed[0]);
    EXPECT_EQ("parent", destroyed[1]);
  }
};

// See description of RunDestroyChildWidgetsTest(). Parent uses
// DesktopNativeWidgetAura.
TEST_F(WidgetChildDestructionTest,
       DestroyChildWidgetsInOrderWithDesktopNativeWidget) {
  RunDestroyChildWidgetsTest(true, false);
}

// See description of RunDestroyChildWidgetsTest(). Both parent and child use
// DesktopNativeWidgetAura.
TEST_F(WidgetChildDestructionTest,
       DestroyChildWidgetsInOrderWithDesktopNativeWidgetForBoth) {
  RunDestroyChildWidgetsTest(true, true);
}

// See description of RunDestroyChildWidgetsTest().
TEST_F(WidgetChildDestructionTest, DestroyChildWidgetsInOrder) {
  RunDestroyChildWidgetsTest(false, false);
}

// Verifies nativeview visbility matches that of Widget visibility when
// SetFullscreen is invoked.
TEST_F(WidgetTest, FullscreenStatePropagated) {
  std::unique_ptr<Widget> top_level_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  top_level_widget->SetFullscreen(true);
  EXPECT_EQ(top_level_widget->IsVisible(),
            IsNativeWindowVisible(top_level_widget->GetNativeWindow()));
}

// Verifies nativeview visbility matches that of Widget visibility when
// SetFullscreen is invoked, for a widget provided with a desktop widget.
TEST_F(DesktopWidgetTest, FullscreenStatePropagated_DesktopWidget) {
  std::unique_ptr<Widget> top_level_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  top_level_widget->SetFullscreen(true);
  EXPECT_EQ(top_level_widget->IsVisible(),
            IsNativeWindowVisible(top_level_widget->GetNativeWindow()));
}

// Used to delete the widget when the supplied bounds changes.
class DestroyingWidgetBoundsObserver : public WidgetObserver {
 public:
  explicit DestroyingWidgetBoundsObserver(Widget* widget) {
    widget_observation_.Observe(widget);
  }

  // There are no assertions here as not all platforms call
  // OnWidgetBoundsChanged() when going fullscreen.
  ~DestroyingWidgetBoundsObserver() override = default;

  // WidgetObserver:
  void OnWidgetBoundsChanged(Widget* widget,
                             const gfx::Rect& new_bounds) override {
    widget_observation_.Reset();
  }

 private:
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

// Deletes a Widget when the bounds change as part of toggling fullscreen.
// This is a regression test for https://crbug.com/1197436.
// Disabled on Mac: This test has historically deleted the Widget not during
// SetFullscreen, but at the end of the test. When the Widget is deleted inside
// SetFullscreen, the test crashes.
// https://crbug.com/1307486
#if BUILDFLAG(IS_MAC)
#define MAYBE_DeleteInSetFullscreen DISABLED_DeleteInSetFullscreen
#else
#define MAYBE_DeleteInSetFullscreen DeleteInSetFullscreen
#endif
TEST_F(DesktopWidgetTest, MAYBE_DeleteInSetFullscreen) {
  std::unique_ptr<Widget> widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  widget->Init(std::move(params));
  Widget* w = widget.get();
  DestroyingWidgetBoundsObserver destroyer(w);
  w->SetFullscreen(true);
}

namespace {

class FullscreenAwareFrame : public views::NonClientFrameView {
  METADATA_HEADER(FullscreenAwareFrame, views::NonClientFrameView)

 public:
  explicit FullscreenAwareFrame(views::Widget* widget) : widget_(widget) {}

  FullscreenAwareFrame(const FullscreenAwareFrame&) = delete;
  FullscreenAwareFrame& operator=(const FullscreenAwareFrame&) = delete;

  ~FullscreenAwareFrame() override = default;

  // views::NonClientFrameView overrides:
  gfx::Rect GetBoundsForClientView() const override { return gfx::Rect(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return gfx::Rect();
  }
  int NonClientHitTest(const gfx::Point& point) override { return HTNOWHERE; }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  // views::View overrides:
  void Layout(PassKey) override {
    if (widget_->IsFullscreen()) {
      fullscreen_layout_called_ = true;
    }
  }

  bool fullscreen_layout_called() const { return fullscreen_layout_called_; }

 private:
  raw_ptr<views::Widget> widget_;
  bool fullscreen_layout_called_ = false;
};

BEGIN_METADATA(FullscreenAwareFrame)
END_METADATA

}  // namespace

// Tests that frame Layout is called when a widget goes fullscreen without
// changing its size or title.
TEST_F(WidgetTest, FullscreenFrameLayout) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  auto frame_view = std::make_unique<FullscreenAwareFrame>(widget.get());
  FullscreenAwareFrame* frame = frame_view.get();
  widget->non_client_view()->SetFrameView(std::move(frame_view));

  widget->Maximize();
  RunPendingMessages();

  EXPECT_FALSE(frame->fullscreen_layout_called());
  widget->SetFullscreen(true);
  widget->Show();
#if BUILDFLAG(IS_MAC)
  // On macOS, a fullscreen layout is triggered from within SetFullscreen.
  // https://crbug.com/1307496
  EXPECT_TRUE(frame->fullscreen_layout_called());
#else
  EXPECT_TRUE(ViewTestApi(frame).needs_layout());
#endif
  widget->LayoutRootViewIfNecessary();
  RunPendingMessages();

  EXPECT_TRUE(frame->fullscreen_layout_called());
}

namespace {

// Trivial WidgetObserverTest that invokes Widget::IsActive() from
// OnWindowDestroying.
class IsActiveFromDestroyObserver : public WidgetObserver {
 public:
  explicit IsActiveFromDestroyObserver(Widget* widget) {
    widget_observation_.Observe(widget);
  }

  IsActiveFromDestroyObserver(const IsActiveFromDestroyObserver&) = delete;
  IsActiveFromDestroyObserver& operator=(const IsActiveFromDestroyObserver&) =
      delete;

  ~IsActiveFromDestroyObserver() override = default;
  void OnWidgetDestroying(Widget* widget) override {
    widget->IsActive();
    widget_observation_.Reset();
  }

 private:
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

}  // namespace

class ChildDesktopWidgetTest : public DesktopWidgetTest {
 public:
  Widget::InitParams CreateParams(Widget::InitParams::Ownership ownership,
                                  Widget::InitParams::Type type) override {
    Widget::InitParams params =
        DesktopWidgetTest::CreateParams(ownership, type);
    if (context_) {
      params.context = context_;
    }
    return params;
  }

  std::unique_ptr<Widget> CreateChildWidget(gfx::NativeWindow context) {
    context_ = context;
    return CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  }

 private:
  gfx::NativeWindow context_ = gfx::NativeWindow();
};

// Verifies Widget::IsActive() invoked from
// WidgetObserver::OnWidgetDestroying() in a child widget doesn't crash.
TEST_F(ChildDesktopWidgetTest, IsActiveFromDestroy) {
  // Create two widgets, one a child of the other.
  std::unique_ptr<Widget> parent_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  parent_widget->Show();

  std::unique_ptr<Widget> child_widget =
      CreateChildWidget(parent_widget->GetNativeWindow());
  IsActiveFromDestroyObserver observer(child_widget.get());
  child_widget->Show();

  parent_widget->CloseNow();
}

// Tests that events propagate through from the dispatcher with the correct
// event type, and that the different platforms behave the same.
TEST_F(WidgetTest, MouseEventTypesViaGenerator) {
  WidgetAutoclosePtr widget(CreateTopLevelFramelessPlatformWidget());
  EventCountView* view =
      widget->GetRootView()->AddChildView(std::make_unique<EventCountView>());
  view->set_handle_mode(EventCountView::CONSUME_EVENTS);
  view->SetBounds(10, 10, 50, 40);

  widget->SetBounds(gfx::Rect(0, 0, 100, 80));
  widget->Show();

  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());
  const gfx::Point view_center_point = view->GetBoundsInScreen().CenterPoint();
  generator->set_current_screen_location(view_center_point);

  generator->ClickLeftButton();
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseReleased));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, view->last_flags());

  generator->PressRightButton();
  EXPECT_EQ(2, view->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseReleased));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, view->last_flags());

  generator->ReleaseRightButton();
  EXPECT_EQ(2, view->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(2, view->GetEventCount(ui::EventType::kMouseReleased));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, view->last_flags());

  // Test mouse move events.
  EXPECT_EQ(0, view->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(0, view->GetEventCount(ui::EventType::kMouseEntered));

  // Move the mouse a displacement of (10, 10).
  generator->MoveMouseTo(view_center_point + gfx::Vector2d(10, 10));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(ui::EF_NONE, view->last_flags());

  // Move it again - entered count shouldn't change.
  generator->MoveMouseTo(view_center_point + gfx::Vector2d(11, 11));
  EXPECT_EQ(2, view->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, view->GetEventCount(ui::EventType::kMouseExited));

  // Move it off the view.
  const gfx::Point out_of_bounds_point =
      view->GetBoundsInScreen().bottom_right() + gfx::Vector2d(10, 10);
  generator->MoveMouseTo(out_of_bounds_point);
  EXPECT_EQ(2, view->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseExited));

  // Move it back on.
  generator->MoveMouseTo(view_center_point);
  EXPECT_EQ(3, view->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(2, view->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseExited));

  // Drargging. Cover HasCapture() and NativeWidgetPrivate::IsMouseButtonDown().
  generator->DragMouseTo(out_of_bounds_point);
  EXPECT_EQ(3, view->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(3, view->GetEventCount(ui::EventType::kMouseReleased));
  EXPECT_EQ(1, view->GetEventCount(ui::EventType::kMouseDragged));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, view->last_flags());
}

// Tests that the root view is correctly set up for Widget types that do not
// require a non-client view, before any other views are added to the widget.
// That is, before Widget::ReorderNativeViews() is called which, if called with
// a root view not set, could cause the root view to get resized to the widget.
TEST_F(WidgetTest, NonClientWindowValidAfterInit) {
  WidgetAutoclosePtr widget(CreateTopLevelFramelessPlatformWidget());
  View* root_view = widget->GetRootView();

  // Size the root view to exceed the widget bounds.
  const gfx::Rect test_rect(0, 0, 500, 500);
  root_view->SetBoundsRect(test_rect);

  EXPECT_NE(test_rect.size(), widget->GetWindowBoundsInScreen().size());

  EXPECT_EQ(test_rect, root_view->bounds());
  widget->ReorderNativeViews();
  EXPECT_EQ(test_rect, root_view->bounds());
}

#if BUILDFLAG(IS_WIN)
// Provides functionality to subclass a window and keep track of messages
// received.
class SubclassWindowHelper {
 public:
  explicit SubclassWindowHelper(HWND window)
      : window_(window), message_to_destroy_on_(0) {
    EXPECT_EQ(instance_, nullptr);
    instance_ = this;
    EXPECT_TRUE(Subclass());
  }

  SubclassWindowHelper(const SubclassWindowHelper&) = delete;
  SubclassWindowHelper& operator=(const SubclassWindowHelper&) = delete;

  ~SubclassWindowHelper() {
    Unsubclass();
    instance_ = nullptr;
  }

  // Returns true if the |message| passed in was received.
  bool received_message(unsigned int message) {
    return (messages_.find(message) != messages_.end());
  }

  void Clear() { messages_.clear(); }

  void set_message_to_destroy_on(unsigned int message) {
    message_to_destroy_on_ = message;
  }

 private:
  bool Subclass() {
    old_proc_ = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(
        window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    return old_proc_ != nullptr;
  }

  void Unsubclass() {
    ::SetWindowLongPtr(window_, GWLP_WNDPROC,
                       reinterpret_cast<LONG_PTR>(old_proc_));
  }

  static LRESULT CALLBACK WndProc(HWND window,
                                  unsigned int message,
                                  WPARAM w_param,
                                  LPARAM l_param) {
    EXPECT_NE(instance_, nullptr);
    EXPECT_EQ(window, instance_->window_);

    // Keep track of messags received for this window.
    instance_->messages_.insert(message);

    LRESULT ret = ::CallWindowProc(instance_->old_proc_, window, message,
                                   w_param, l_param);
    if (message == instance_->message_to_destroy_on_) {
      instance_->Unsubclass();
      ::DestroyWindow(window);
    }
    return ret;
  }

  WNDPROC old_proc_;
  HWND window_;
  static SubclassWindowHelper* instance_;
  std::set<unsigned int> messages_;
  unsigned int message_to_destroy_on_;
};

SubclassWindowHelper* SubclassWindowHelper::instance_ = nullptr;

// This test validates whether the WM_SYSCOMMAND message for SC_MOVE is
// received when we post a WM_NCLBUTTONDOWN message for the caption in the
// following scenarios:-
// 1. Posting a WM_NCMOUSEMOVE message for a different location.
// 2. Posting a WM_NCMOUSEMOVE message with a different hittest code.
// 3. Posting a WM_MOUSEMOVE message.
// Disabled because of flaky timeouts: http://crbug.com/592742
TEST_F(DesktopWidgetTest,
       DISABLED_SysCommandMoveOnNCLButtonDownOnCaptionAndMoveTest) {
  std::unique_ptr<Widget> widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Show();
  ::SetCursorPos(500, 500);

  HWND window = widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget();

  SubclassWindowHelper subclass_helper(window);

  // Posting just a WM_NCLBUTTONDOWN message should not result in a
  // WM_SYSCOMMAND
  ::PostMessage(window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(100, 100));
  RunPendingMessages();
  EXPECT_TRUE(subclass_helper.received_message(WM_NCLBUTTONDOWN));
  EXPECT_FALSE(subclass_helper.received_message(WM_SYSCOMMAND));

  subclass_helper.Clear();
  // Posting a WM_NCLBUTTONDOWN message followed by a WM_NCMOUSEMOVE at the
  // same location should not result in a WM_SYSCOMMAND message.
  ::PostMessage(window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(100, 100));
  ::PostMessage(window, WM_NCMOUSEMOVE, HTCAPTION, MAKELPARAM(100, 100));
  RunPendingMessages();

  EXPECT_TRUE(subclass_helper.received_message(WM_NCLBUTTONDOWN));
  EXPECT_TRUE(subclass_helper.received_message(WM_NCMOUSEMOVE));
  EXPECT_FALSE(subclass_helper.received_message(WM_SYSCOMMAND));

  subclass_helper.Clear();
  // Posting a WM_NCLBUTTONDOWN message followed by a WM_NCMOUSEMOVE at a
  // different location should result in a WM_SYSCOMMAND message.
  ::PostMessage(window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(100, 100));
  ::PostMessage(window, WM_NCMOUSEMOVE, HTCAPTION, MAKELPARAM(110, 110));
  RunPendingMessages();

  EXPECT_TRUE(subclass_helper.received_message(WM_NCLBUTTONDOWN));
  EXPECT_TRUE(subclass_helper.received_message(WM_NCMOUSEMOVE));
  EXPECT_TRUE(subclass_helper.received_message(WM_SYSCOMMAND));

  subclass_helper.Clear();
  // Posting a WM_NCLBUTTONDOWN message followed by a WM_NCMOUSEMOVE at a
  // different location with a different hittest code should result in a
  // WM_SYSCOMMAND message.
  ::PostMessage(window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(100, 100));
  ::PostMessage(window, WM_NCMOUSEMOVE, HTTOP, MAKELPARAM(110, 102));
  RunPendingMessages();

  EXPECT_TRUE(subclass_helper.received_message(WM_NCLBUTTONDOWN));
  EXPECT_TRUE(subclass_helper.received_message(WM_NCMOUSEMOVE));
  EXPECT_TRUE(subclass_helper.received_message(WM_SYSCOMMAND));

  subclass_helper.Clear();
  // Posting a WM_NCLBUTTONDOWN message followed by a WM_MOUSEMOVE should
  // result in a WM_SYSCOMMAND message.
  ::PostMessage(window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(100, 100));
  ::PostMessage(window, WM_MOUSEMOVE, HTCLIENT, MAKELPARAM(110, 110));
  RunPendingMessages();

  EXPECT_TRUE(subclass_helper.received_message(WM_NCLBUTTONDOWN));
  EXPECT_TRUE(subclass_helper.received_message(WM_MOUSEMOVE));
  EXPECT_TRUE(subclass_helper.received_message(WM_SYSCOMMAND));
}

// This test validates that destroying the window in the context of the
// WM_SYSCOMMAND message with SC_MOVE does not crash.
// Disabled because of flaky timeouts: http://crbug.com/592742
TEST_F(DesktopWidgetTest, DISABLED_DestroyInSysCommandNCLButtonDownOnCaption) {
  std::unique_ptr<Widget> widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Show();
  ::SetCursorPos(500, 500);

  HWND window = widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget();

  SubclassWindowHelper subclass_helper(window);

  // Destroying the window in the context of the WM_SYSCOMMAND message
  // should not crash.
  subclass_helper.set_message_to_destroy_on(WM_SYSCOMMAND);

  ::PostMessage(window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(100, 100));
  ::PostMessage(window, WM_NCMOUSEMOVE, HTCAPTION, MAKELPARAM(110, 110));
  RunPendingMessages();

  EXPECT_TRUE(subclass_helper.received_message(WM_NCLBUTTONDOWN));
  EXPECT_TRUE(subclass_helper.received_message(WM_SYSCOMMAND));
}

#endif

// Test that the z-order levels round-trip.
TEST_F(WidgetTest, ZOrderLevel) {
  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());
  EXPECT_EQ(ui::ZOrderLevel::kNormal, widget->GetZOrderLevel());
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow, widget->GetZOrderLevel());
  widget->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  EXPECT_EQ(ui::ZOrderLevel::kNormal, widget->GetZOrderLevel());
}

namespace {

class ScaleFactorView : public View {
  METADATA_HEADER(ScaleFactorView, View)

 public:
  ScaleFactorView() = default;

  ScaleFactorView(const ScaleFactorView&) = delete;
  ScaleFactorView& operator=(const ScaleFactorView&) = delete;

  // Overridden from ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {
    last_scale_factor_ = new_device_scale_factor;
    View::OnDeviceScaleFactorChanged(old_device_scale_factor,
                                     new_device_scale_factor);
  }

  float last_scale_factor() const { return last_scale_factor_; }

 private:
  float last_scale_factor_ = 0.f;
};

BEGIN_METADATA(ScaleFactorView)
END_METADATA

}  // namespace

// Ensure scale factor changes are propagated from the native Widget.
TEST_F(WidgetTest, OnDeviceScaleFactorChanged) {
  // Automatically close the widget, but not delete it.
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  ScaleFactorView* view = new ScaleFactorView;
  widget->GetRootView()->AddChildView(view);
  float scale_factor = widget->GetLayer()->device_scale_factor();
  EXPECT_NE(scale_factor, 0.f);

  // For views that are not layer-backed, adding the view won't notify the view
  // about the initial scale factor. Fake it.
  view->OnDeviceScaleFactorChanged(0.f, scale_factor);
  EXPECT_EQ(scale_factor, view->last_scale_factor());

  // Changes should be propagated.
  scale_factor *= 2.0f;
  widget->GetLayer()->OnDeviceScaleFactorChanged(scale_factor);
  EXPECT_EQ(scale_factor, view->last_scale_factor());
}

// Test that WidgetRemovalsObserver::OnWillRemoveView is called when deleting
// a view.
TEST_F(WidgetTest, WidgetRemovalsObserverCalled) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  TestWidgetRemovalsObserver removals_observer;
  widget->AddRemovalsObserver(&removals_observer);

  View* parent = new View();
  widget->client_view()->AddChildView(parent);

  View* child = new View();
  parent->AddChildView(child);

  widget->client_view()->RemoveChildView(parent);
  EXPECT_TRUE(removals_observer.DidRemoveView(parent));
  EXPECT_FALSE(removals_observer.DidRemoveView(child));

  // Calling RemoveChildView() doesn't delete the view, but deleting
  // |parent| will automatically delete |child|.
  delete parent;

  widget->RemoveRemovalsObserver(&removals_observer);
}

// Test that WidgetRemovalsObserver::OnWillRemoveView is called when deleting
// the root view.
TEST_F(WidgetTest, WidgetRemovalsObserverCalledWhenRemovingRootView) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  TestWidgetRemovalsObserver removals_observer;
  widget->AddRemovalsObserver(&removals_observer);
  views::View* root_view = widget->GetRootView();

  widget.reset();
  EXPECT_TRUE(removals_observer.DidRemoveView(root_view));
}

// Test that WidgetRemovalsObserver::OnWillRemoveView is called when moving
// a view from one widget to another, but not when moving a view within
// the same widget.
TEST_F(WidgetTest, WidgetRemovalsObserverCalledWhenMovingBetweenWidgets) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  TestWidgetRemovalsObserver removals_observer;
  widget->AddRemovalsObserver(&removals_observer);

  View* parent = new View();
  widget->client_view()->AddChildView(parent);

  View* child = new View();
  widget->client_view()->AddChildView(child);

  // Reparenting the child shouldn't call the removals observer.
  parent->AddChildView(child);
  EXPECT_FALSE(removals_observer.DidRemoveView(child));

  // Moving the child to a different widget should call the removals observer.
  WidgetAutoclosePtr widget2(CreateTopLevelPlatformWidget());
  widget2->client_view()->AddChildView(child);
  EXPECT_TRUE(removals_observer.DidRemoveView(child));

  widget->RemoveRemovalsObserver(&removals_observer);
}

// Test dispatch of ui::EventType::kMousewheel.
TEST_F(WidgetTest, MouseWheelEvent) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->SetBounds(gfx::Rect(0, 0, 600, 600));
  EventCountView* event_count_view =
      widget->client_view()->AddChildView(std::make_unique<EventCountView>());
  event_count_view->SetBounds(0, 0, 600, 600);
  widget->Show();

  auto event_generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());

  event_generator->MoveMouseWheel(1, 1);
  EXPECT_EQ(1, event_count_view->GetEventCount(ui::EventType::kMousewheel));
}

// Test that ui::EventType::kMouseEntered is dispatched even when not followed
// by ui::EventType::kMouseMoved.
TEST_F(WidgetTest, MouseEnteredWithoutMoved) {
  WidgetAutoclosePtr widget(CreateTopLevelFramelessPlatformWidget());
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));

  auto* root_view = static_cast<internal::RootView*>(widget->GetRootView());
  auto* v1 = root_view->AddChildView(std::make_unique<EventCountView>());
  v1->SetBounds(10, 10, 10, 10);
  auto* v2 = root_view->AddChildView(std::make_unique<EventCountView>());
  v2->SetBounds(20, 10, 10, 10);

  widget->Show();

  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());

  // Enter |v1| and check that it received ui::EventType::kMouseEntered.
  auto enter_location = v1->GetBoundsInScreen().CenterPoint();
  generator->set_current_screen_location(enter_location);
  generator->SendMouseEnter();
  EXPECT_EQ(v1, GetMouseMoveHandler(root_view));
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kMouseExited));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseExited));

  // Enter |v2| and check that |v1| received ui::EventType::kMouseExited and
  // |v2| received ui::EventType::kMouseEntered.
  enter_location = v2->GetBoundsInScreen().CenterPoint();
  generator->set_current_screen_location(enter_location);
  generator->SendMouseEnter();
  EXPECT_EQ(v2, GetMouseMoveHandler(root_view));
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kMouseExited));
  EXPECT_EQ(1, v2->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseExited));

  // Enter |root_view| and check that |v2| received ui::EventType::kMouseExited.
  enter_location = gfx::Point(0, 0);
  generator->set_current_screen_location(enter_location);
  generator->SendMouseEnter();
  EXPECT_EQ(nullptr, GetMouseMoveHandler(root_view));
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kMouseExited));
  EXPECT_EQ(1, v2->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMouseMoved));
  EXPECT_EQ(1, v2->GetEventCount(ui::EventType::kMouseExited));
}

// Test that ui::EventType::kMouseMoved after ui::EventType::kMouseEntered
// doesn't cause an extra ui::EventType::kMouseEntered.
TEST_F(WidgetTest, MouseMovedAfterEnteredDoesntCauseEntered) {
  WidgetAutoclosePtr widget(CreateTopLevelFramelessPlatformWidget());
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));

  auto* root_view = widget->GetRootView();
  auto* v = root_view->AddChildView(std::make_unique<EventCountView>());
  v->SetBounds(10, 10, 10, 10);

  widget->Show();

  auto generator =
      CreateEventGenerator(GetContext(), widget->GetNativeWindow());

  // Enter |v|.
  auto enter_location = v->GetBoundsInScreen().CenterPoint();
  generator->set_current_screen_location(enter_location);
  generator->SendMouseEnter();

  // Send ui::EventType::kMouseMoved at the same location and check that it
  // didn't generate ui::EventType::kMouseEntered again.
  generator->MoveMouseBy(0, 0);
  EXPECT_EQ(1, v->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(1, v->GetEventCount(ui::EventType::kMouseMoved));

  // Reset state by entering |root_view|.
  generator->MoveMouseTo(0, 0);

  // Enter |v| again.
  generator->set_current_screen_location(enter_location);
  generator->SendMouseEnter();

  // Send ui::EventType::kMouseMoved at a slightly offset location and check
  // that it didn't generate ui::EventType::kMouseEntered again.
  generator->MoveMouseBy(1, 1);
  EXPECT_EQ(2, v->GetEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(2, v->GetEventCount(ui::EventType::kMouseMoved));
}

class CloseFromClosingObserver : public WidgetObserver {
 public:
  ~CloseFromClosingObserver() override {
    EXPECT_TRUE(was_on_widget_closing_called_);
  }
  // WidgetObserver:
  void OnWidgetClosing(Widget* widget) override {
    // OnWidgetClosing() should only be called once, even if Close() is called
    // after CloseNow().
    ASSERT_FALSE(was_on_widget_closing_called_);
    was_on_widget_closing_called_ = true;
    widget->Close();
  }

 private:
  bool was_on_widget_closing_called_ = false;
};

TEST_F(WidgetTest, CloseNowFollowedByCloseDoesntCallOnWidgetClosingTwice) {
  CloseFromClosingObserver observer;
  std::unique_ptr<Widget> widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->AddObserver(&observer);
  widget->CloseNow();
  widget->RemoveObserver(&observer);
  widget.reset();
  // Assertions are in CloseFromClosingObserver.
}

namespace {

class TestSaveWindowPlacementWidgetDelegate : public TestDesktopWidgetDelegate {
 public:
  TestSaveWindowPlacementWidgetDelegate() = default;
  TestSaveWindowPlacementWidgetDelegate(
      const TestSaveWindowPlacementWidgetDelegate&) = delete;
  TestSaveWindowPlacementWidgetDelegate operator=(
      const TestSaveWindowPlacementWidgetDelegate&) = delete;
  ~TestSaveWindowPlacementWidgetDelegate() override = default;

  void set_should_save_window_placement(bool should_save) {
    should_save_window_placement_ = should_save;
  }

  int save_window_placement_count() const {
    return save_window_placement_count_;
  }

  // ViewsDelegate:
  std::string GetWindowName() const final { return GetWidget()->GetName(); }
  bool ShouldSaveWindowPlacement() const final {
    return should_save_window_placement_;
  }
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::mojom::WindowShowState show_state) override {
    save_window_placement_count_++;
  }

 private:
  bool should_save_window_placement_ = true;
  int save_window_placement_count_ = 0;
};

}  // namespace

TEST_F(WidgetTest, ShouldSaveWindowPlacement) {
  for (bool save : {false, true}) {
    SCOPED_TRACE(save ? "ShouldSave" : "ShouldNotSave");
    TestSaveWindowPlacementWidgetDelegate widget_delegate;
    widget_delegate.set_should_save_window_placement(save);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
    params.name = "TestWidget";
    widget_delegate.InitWidget(std::move(params));
    auto* widget = widget_delegate.GetWidget();
    widget->Close();
    EXPECT_EQ(save ? 1 : 0, widget_delegate.save_window_placement_count());
  }
}

TEST_F(WidgetTest, RootViewAccessibilityCacheInitialized) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();

  EXPECT_TRUE(widget->GetRootView()->GetViewAccessibility().is_initialized());
}

TEST_F(WidgetTest, ClientViewAccessibilityProperties) {
  std::unique_ptr<Widget> widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Show();

  ui::AXNodeData node_data;
  widget->client_view()->GetViewAccessibility().GetAccessibleNodeData(
      &node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kClient);
}

TEST_F(WidgetTest, NonClientViewAccessibilityProperties) {
  std::unique_ptr<Widget> widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  NonClientView* non_client_view = widget->non_client_view();
  non_client_view->SetFrameView(
      std::make_unique<MinimumSizeFrameView>(widget.get()));
  widget->Show();

  ui::AXNodeData node_data;
  non_client_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kClient);

  node_data = ui::AXNodeData();
  non_client_view->frame_view()->GetViewAccessibility().GetAccessibleNodeData(
      &node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kClient);
}

// Parameterized test that verifies the behavior of SetAspectRatio with respect
// to the excluded margin.
class WidgetSetAspectRatioTest
    : public ViewsTestBase,
      public testing::WithParamInterface<gfx::Size /* margin */> {
 public:
  WidgetSetAspectRatioTest() : margin_(GetParam()) {}

  WidgetSetAspectRatioTest(const WidgetSetAspectRatioTest&) = delete;
  WidgetSetAspectRatioTest& operator=(const WidgetSetAspectRatioTest&) = delete;

  ~WidgetSetAspectRatioTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    native_widget_ = std::make_unique<MockNativeWidget>(widget());
    ON_CALL(*native_widget(), CreateNonClientFrameView).WillByDefault([this]() {
      return std::make_unique<NonClientFrameViewWithFixedMargin>(margin());
    });
    params.native_widget = native_widget();
    widget()->Init(std::move(params));
    task_environment()->RunUntilIdle();
  }

  void TearDown() override {
    // `ViewAccessibility` objects have some references to the `widget` which
    // must be updated when the widget is freed. The function that is in charge
    // of clearing these lists however (`OnNativeWidgetDestroying`), is never
    // called in this test suite because we use a `MockNativeWindow` rather than
    // a `NativeWindow`. So we make sure this clean up happens manually.
    widget()->OnNativeWidgetDestroying();
    native_widget_.reset();
    widget()->Close();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  const gfx::Size& margin() const { return margin_; }
  Widget* widget() { return widget_.get(); }
  MockNativeWidget* native_widget() { return native_widget_.get(); }

 private:
  // Margin around the client view that should be excluded.
  const gfx::Size margin_;
  std::unique_ptr<Widget> widget_;
  std::unique_ptr<MockNativeWidget> native_widget_;

  // `NonClientFrameView` that pads the client view with a fixed-size margin,
  // to leave room for drawing that's not included in the aspect ratio.
  class NonClientFrameViewWithFixedMargin : public NonClientFrameView {
   public:
    // `margin` is the margin that we'll provide to our client view.
    explicit NonClientFrameViewWithFixedMargin(const gfx::Size& margin)
        : margin_(margin) {}

    // NonClientFrameView
    gfx::Rect GetBoundsForClientView() const override {
      gfx::Rect r = bounds();
      return gfx::Rect(r.x(), r.y(), r.width() - margin_.width(),
                       r.height() - margin_.height());
    }

    const gfx::Size margin_;
  };
};

TEST_P(WidgetSetAspectRatioTest, SetAspectRatioIncludesMargin) {
  // Provide a nonzero size.  It doesn't particularly matter what, as long as
  // it's larger than our margin.
  const gfx::Rect root_view_bounds(0, 0, 100, 200);
  ASSERT_GT(root_view_bounds.width(), margin().width());
  ASSERT_GT(root_view_bounds.height(), margin().height());
  widget()->non_client_view()->SetBoundsRect(root_view_bounds);

  // Verify that the excluded margin matches the margin that our custom
  // non-client frame provides.
  const gfx::SizeF aspect_ratio(1.5f, 1.0f);
  EXPECT_CALL(*native_widget(), SetAspectRatio(aspect_ratio, margin()));
  widget()->SetAspectRatio(aspect_ratio);
}

INSTANTIATE_TEST_SUITE_P(WidgetSetAspectRatioTestInstantiation,
                         WidgetSetAspectRatioTest,
                         ::testing::Values(gfx::Size(15, 20), gfx::Size(0, 0)));

class WidgetShadowTest : public WidgetTest {
 public:
  WidgetShadowTest() = default;

  WidgetShadowTest(const WidgetShadowTest&) = delete;
  WidgetShadowTest& operator=(const WidgetShadowTest&) = delete;

  ~WidgetShadowTest() override = default;

  // WidgetTest:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    WidgetTest::SetUp();
    InitControllers();
  }

  void TearDown() override {
#if defined(USE_AURA) && !BUILDFLAG(ENABLE_DESKTOP_AURA)
    shadow_controller_.reset();
    focus_controller_.reset();
#endif
    WidgetTest::TearDown();
  }

  Widget::InitParams CreateParams(Widget::InitParams::Ownership ownership,
                                  Widget::InitParams::Type type) override {
    Widget::InitParams params =
        WidgetTest::CreateParams(ownership, override_type_.value_or(type));
    params.shadow_type = Widget::InitParams::ShadowType::kDrop;
    params.shadow_elevation = 10;
    params.name = name_;
    params.child = force_child_;
    return params;
  }

 protected:
  std::optional<Widget::InitParams::Type> override_type_;
  std::string name_;
  bool force_child_ = false;

 private:
#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)
  void InitControllers() {}
#else
  class TestFocusRules : public wm::BaseFocusRules {
   public:
    TestFocusRules() = default;

    TestFocusRules(const TestFocusRules&) = delete;
    TestFocusRules& operator=(const TestFocusRules&) = delete;

    bool SupportsChildActivation(const aura::Window* window) const override {
      return true;
    }
  };

  void InitControllers() {
    focus_controller_ =
        std::make_unique<wm::FocusController>(new TestFocusRules);
    shadow_controller_ = std::make_unique<wm::ShadowController>(
        focus_controller_.get(), nullptr);
  }

  std::unique_ptr<wm::FocusController> focus_controller_;
  std::unique_ptr<wm::ShadowController> shadow_controller_;
#endif  // !BUILDFLAG(ENABLE_DESKTOP_AURA) && !BUILDFLAG(IS_MAC)
};

// Disabled on Mac: All drop shadows are managed out of process for now.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShadowsInRootWindow DISABLED_ShadowsInRootWindow
#else
#define MAYBE_ShadowsInRootWindow ShadowsInRootWindow
#endif

// Test that shadows are not added to root windows when created or upon
// activation. Test that shadows are added to non-root windows even if not
// activated.
TEST_F(WidgetShadowTest, MAYBE_ShadowsInRootWindow) {
#if defined(USE_AURA) && !BUILDFLAG(ENABLE_DESKTOP_AURA)
  // On ChromeOS, top-levels have shadows.
  bool top_level_window_should_have_shadow = true;
#else
  // On non-chromeos platforms, the hosting OS is responsible for the shadow.
  bool top_level_window_should_have_shadow = false;
#endif

  // To start, just create a Widget. This constructs the first ShadowController
  // which will start observing the environment for additional aura::Window
  // initialization. The very first ShadowController in DesktopNativeWidgetAura
  // is created after the call to aura::Window::Init(), so the ShadowController
  // Impl class won't ever see this first Window being initialized.
  name_ = "other_top_level";
  Widget* other_top_level = CreateTopLevelNativeWidget();

  name_ = "top_level";
  Widget* top_level = CreateTopLevelNativeWidget();
  top_level->SetBounds(gfx::Rect(100, 100, 320, 200));

  EXPECT_FALSE(WidgetHasInProcessShadow(top_level));
  EXPECT_FALSE(top_level->IsVisible());
  top_level->ShowInactive();
  EXPECT_EQ(top_level_window_should_have_shadow,
            WidgetHasInProcessShadow(top_level));
  top_level->Show();
  EXPECT_EQ(top_level_window_should_have_shadow,
            WidgetHasInProcessShadow(top_level));

  name_ = "control";
  Widget* control = CreateChildNativeWidgetWithParent(top_level);
  control->SetBounds(gfx::Rect(20, 20, 160, 100));

  // Widgets of TYPE_CONTROL become visible during Init, so start with a shadow.
  EXPECT_TRUE(WidgetHasInProcessShadow(control));
  control->ShowInactive();
  EXPECT_TRUE(WidgetHasInProcessShadow(control));
  control->Show();
  EXPECT_TRUE(WidgetHasInProcessShadow(control));

  name_ = "child";
  override_type_ = Widget::InitParams::TYPE_POPUP;
  force_child_ = true;
  Widget* child = CreateChildNativeWidgetWithParent(top_level);
  child->SetBounds(gfx::Rect(20, 20, 160, 100));

  // Now false: the Widget hasn't been shown yet.
  EXPECT_FALSE(WidgetHasInProcessShadow(child));
  child->ShowInactive();
  EXPECT_TRUE(WidgetHasInProcessShadow(child));
  child->Show();
  EXPECT_TRUE(WidgetHasInProcessShadow(child));

  other_top_level->Show();

  // Re-activate the top level window. This handles a hypothetical case where
  // a shadow is added via the ActivationChangeObserver rather than by the
  // aura::WindowObserver. Activation changes only modify an existing shadow
  // (if there is one), but should never install a Shadow, even if the Window
  // properties otherwise say it should have one.
  top_level->Show();
  EXPECT_EQ(top_level_window_should_have_shadow,
            WidgetHasInProcessShadow(top_level));

  top_level->Close();
  other_top_level->Close();
}

#if BUILDFLAG(IS_WIN)

// Tests the case where an intervening owner popup window is destroyed out from
// under the currently active modal top-level window. In this instance, the
// remaining top-level windows should be re-enabled.
TEST_F(DesktopWidgetTest, WindowModalOwnerDestroyedEnabledTest) {
  // top_level_widget owns owner_dialog_widget which owns owned_dialog_widget.
  std::unique_ptr<Widget> top_level_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  top_level_widget->Show();

  // Create the owner modal dialog.
  const auto create_params = [this](Widget* widget, gfx::NativeView parent) {
    Widget::InitParams init_params =
        CreateParamsForTestWidget(Widget::InitParams::TYPE_WINDOW);
    init_params.delegate = new WidgetDelegate();
    init_params.delegate->SetModalType(ui::mojom::ModalType::kWindow);
    init_params.parent = parent;
    init_params.native_widget =
        new test::TestPlatformNativeWidget<DesktopNativeWidgetAura>(
            widget, false, nullptr);
    return init_params;
  };
  Widget owner_dialog_widget;
  owner_dialog_widget.Init(
      create_params(&owner_dialog_widget, top_level_widget->GetNativeView()));
  owner_dialog_widget.Show();
  HWND owner_hwnd = HWNDForWidget(&owner_dialog_widget);

  // Create the owned modal dialog.
  Widget owned_dialog_widget;
  owned_dialog_widget.Init(
      create_params(&owned_dialog_widget, owner_dialog_widget.GetNativeView()));
  owned_dialog_widget.Show();
  HWND owned_hwnd = HWNDForWidget(&owned_dialog_widget);

  RunPendingMessages();

  HWND top_hwnd = HWNDForWidget(top_level_widget.get());

  EXPECT_FALSE(!!IsWindowEnabled(owner_hwnd));
  EXPECT_FALSE(!!IsWindowEnabled(top_hwnd));
  EXPECT_TRUE(!!IsWindowEnabled(owned_hwnd));

  owner_dialog_widget.CloseNow();
  RunPendingMessages();

  EXPECT_FALSE(!!IsWindow(owner_hwnd));
  EXPECT_FALSE(!!IsWindow(owned_hwnd));
  EXPECT_TRUE(!!IsWindowEnabled(top_hwnd));

  top_level_widget->CloseNow();
}

TEST_F(DesktopWidgetTest, StackAboveTest) {
  WidgetAutoclosePtr root_one(CreateTopLevelNativeWidget());
  WidgetAutoclosePtr root_two(CreateTopLevelNativeWidget());
  Widget* child_one = CreateChildNativeWidgetWithParent(root_one->AsWidget());
  Widget* child_one_b = CreateChildNativeWidgetWithParent(root_one->AsWidget());
  Widget* child_two = CreateChildNativeWidgetWithParent(root_two->AsWidget());
  Widget* grandchild_one =
      CreateChildNativeWidgetWithParent(child_one->AsWidget());
  Widget* grandchild_two =
      CreateChildNativeWidgetWithParent(child_two->AsWidget());

  root_one->ShowInactive();
  child_one->ShowInactive();
  child_one_b->ShowInactive();
  grandchild_one->ShowInactive();
  root_two->ShowInactive();
  child_two->ShowInactive();
  grandchild_two->ShowInactive();

  // Creates the following where Z-Order is from Left to Right.
  //            root_one                    root_two
  //             /    \                         /
  //       child_one_b  child_one           child_two
  //                       /                  /
  //                 grandchild_one    grandchild_two
  //
  // Note: child_one and grandchild_one were brought to front
  //       when grandchild_one was shown.

  // Child elements are stacked above parent.
  EXPECT_TRUE(child_one->IsStackedAbove(root_one->GetNativeView()));
  EXPECT_TRUE(child_one_b->IsStackedAbove(root_one->GetNativeView()));
  EXPECT_TRUE(grandchild_one->IsStackedAbove(child_one->GetNativeView()));
  EXPECT_TRUE(grandchild_two->IsStackedAbove(root_two->GetNativeView()));

  // Siblings with higher z-order are stacked correctly.
  EXPECT_TRUE(child_one->IsStackedAbove(child_one_b->GetNativeView()));
  EXPECT_TRUE(grandchild_one->IsStackedAbove(child_one_b->GetNativeView()));

  // Root elements are stacked above child of a root with lower z-order.
  EXPECT_TRUE(root_two->IsStackedAbove(root_one->GetNativeView()));
  EXPECT_TRUE(root_two->IsStackedAbove(child_one_b->GetNativeView()));

  // Child elements are stacked above child of root with lower z-order.
  EXPECT_TRUE(child_two->IsStackedAbove(child_one_b->GetNativeView()));
  EXPECT_TRUE(child_two->IsStackedAbove(grandchild_one->GetNativeView()));
  EXPECT_TRUE(grandchild_two->IsStackedAbove(child_one->GetNativeView()));
  EXPECT_TRUE(grandchild_two->IsStackedAbove(root_one->GetNativeView()));

  // False cases to verify function is not just returning true for all cases.
  EXPECT_FALSE(root_one->IsStackedAbove(grandchild_two->GetNativeView()));
  EXPECT_FALSE(root_one->IsStackedAbove(grandchild_one->GetNativeView()));
  EXPECT_FALSE(child_two->IsStackedAbove(grandchild_two->GetNativeView()));
  EXPECT_FALSE(child_one->IsStackedAbove(grandchild_two->GetNativeView()));
  EXPECT_FALSE(child_one_b->IsStackedAbove(child_two->GetNativeView()));
  EXPECT_FALSE(grandchild_one->IsStackedAbove(grandchild_two->GetNativeView()));
  EXPECT_FALSE(grandchild_one->IsStackedAbove(root_two->GetNativeView()));
  EXPECT_FALSE(child_one_b->IsStackedAbove(grandchild_one->GetNativeView()));
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)

namespace {

class CompositingWidgetTest : public DesktopWidgetTest {
 public:
  CompositingWidgetTest()
      : widget_types_{Widget::InitParams::TYPE_WINDOW,
                      Widget::InitParams::TYPE_WINDOW_FRAMELESS,
                      Widget::InitParams::TYPE_CONTROL,
                      Widget::InitParams::TYPE_POPUP,
                      Widget::InitParams::TYPE_MENU,
                      Widget::InitParams::TYPE_TOOLTIP,
                      Widget::InitParams::TYPE_BUBBLE,
                      Widget::InitParams::TYPE_DRAG} {}

  CompositingWidgetTest(const CompositingWidgetTest&) = delete;
  CompositingWidgetTest& operator=(const CompositingWidgetTest&) = delete;

  ~CompositingWidgetTest() override = default;

  Widget::InitParams CreateParams(Widget::InitParams::Ownership ownership,
                                  Widget::InitParams::Type type) override {
    Widget::InitParams params =
        DesktopWidgetTest::CreateParams(ownership, type);
    params.opacity = opacity_;
    return params;
  }

  void CheckAllWidgetsForOpacity(
      const Widget::InitParams::WindowOpacity opacity) {
    opacity_ = opacity;
    for (const auto& widget_type : widget_types_) {
#if BUILDFLAG(IS_MAC)
      // Tooltips are native on Mac. See NativeWidgetNSWindowBridge::Init.
      if (widget_type == Widget::InitParams::TYPE_TOOLTIP) {
        continue;
      }
#elif BUILDFLAG(IS_WIN)
      // Other widget types would require to create a parent window and the
      // the purpose of this test is mainly X11 in the first place.
      if (widget_type != Widget::InitParams::TYPE_WINDOW) {
        continue;
      }
#endif
      std::unique_ptr<Widget> widget =
          CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET, widget_type);

      // Use NativeWidgetAura directly.
      if (widget_type == Widget::InitParams::TYPE_WINDOW_FRAMELESS ||
          widget_type == Widget::InitParams::TYPE_CONTROL) {
        continue;
      }

#if BUILDFLAG(IS_MAC)
      // Mac always always has a compositing window manager, but doesn't have
      // transparent titlebars which is what ShouldWindowContentsBeTransparent()
      // is currently used for. Asking for transparency should get it. Note that
      // TestViewsDelegate::use_transparent_windows_ determines the result of
      // kInferOpacity: assume it is false.
      bool should_be_transparent =
          opacity_ == Widget::InitParams::WindowOpacity::kTranslucent;
#else
      bool should_be_transparent = widget->ShouldWindowContentsBeTransparent();
#endif

      EXPECT_EQ(IsNativeWindowTransparent(widget->GetNativeWindow()),
                should_be_transparent);
    }
  }

 protected:
  const std::vector<Widget::InitParams::Type> widget_types_;
  Widget::InitParams::WindowOpacity opacity_ =
      Widget::InitParams::WindowOpacity::kInferred;
};

}  // namespace

// Only test manually set opacity via kOpaque or kTranslucent.  kInferred is
// unpredictable and depends on the platform and window type.
TEST_F(CompositingWidgetTest, Transparency_DesktopWidgetOpaque) {
  CheckAllWidgetsForOpacity(Widget::InitParams::WindowOpacity::kOpaque);
}

TEST_F(CompositingWidgetTest, Transparency_DesktopWidgetTranslucent) {
  CheckAllWidgetsForOpacity(Widget::InitParams::WindowOpacity::kTranslucent);
}

namespace {

class ScreenshotWidgetTest : public ViewsTestBase {
 public:
  ScreenshotWidgetTest() = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = WidgetAutoclosePtr(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    native_widget_ = std::make_unique<MockNativeWidget>(widget());
    ON_CALL(*native_widget(), SetAllowScreenshots(_))
        .WillByDefault([this](bool allowed) {
          screenshots_allowed_ = allowed;
          return true;
        });
    params.native_widget = native_widget();
    widget()->Init(std::move(params));
  }

  Widget* widget() { return widget_.get(); }
  MockNativeWidget* native_widget() { return native_widget_.get(); }
  const std::optional<bool>& screenshots_allowed() {
    return screenshots_allowed_;
  }

 private:
  WidgetAutoclosePtr widget_;
  std::unique_ptr<MockNativeWidget> native_widget_;
  std::optional<bool> screenshots_allowed_;
};

}  // namespace

TEST_F(ScreenshotWidgetTest, CallsNativeWidget) {
  widget()->SetAllowScreenshots(false);
  ASSERT_TRUE(screenshots_allowed().has_value());
  EXPECT_FALSE(screenshots_allowed().value());
}

#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)

}  // namespace views::test
