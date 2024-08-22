// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scroll_view.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_test_api.h"
#include "ui/views/view_tracker.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

enum ScrollBarOrientation { HORIZONTAL, VERTICAL };

namespace views {
namespace test {

class ScrollViewTestApi {
 public:
  explicit ScrollViewTestApi(ScrollView* scroll_view)
      : scroll_view_(scroll_view) {}
  ScrollViewTestApi(const ScrollViewTestApi&) = delete;
  ScrollViewTestApi& operator=(const ScrollViewTestApi&) = delete;
  ~ScrollViewTestApi() = default;

  ScrollBar* GetScrollBar(ScrollBarOrientation orientation) {
    ScrollBar* scroll_bar = orientation == VERTICAL
                                ? scroll_view_->vertical_scroll_bar()
                                : scroll_view_->horizontal_scroll_bar();
    return scroll_bar;
  }

  const base::OneShotTimer& GetScrollBarTimer(
      ScrollBarOrientation orientation) {
    return GetScrollBar(orientation)->repeater_.timer_for_testing();
  }

  BaseScrollBarThumb* GetScrollBarThumb(ScrollBarOrientation orientation) {
    return GetScrollBar(orientation)->thumb_;
  }

  gfx::Vector2d IntegralViewOffset() {
    return gfx::Point() - gfx::ToFlooredPoint(CurrentOffset());
  }

  gfx::PointF CurrentOffset() const { return scroll_view_->CurrentOffset(); }

  base::RetainingOneShotTimer* GetScrollBarHideTimer(
      ScrollBarOrientation orientation) {
    return ScrollBar::GetHideTimerForTesting(GetScrollBar(orientation));
  }

  View* corner_view() { return scroll_view_->corner_view_.get(); }
  View* contents_viewport() {
    return scroll_view_->GetContentsViewportForTest();
  }

  View* more_content_left() { return scroll_view_->more_content_left_.get(); }
  View* more_content_top() { return scroll_view_->more_content_top_.get(); }
  View* more_content_right() { return scroll_view_->more_content_right_.get(); }
  View* more_content_bottom() {
    return scroll_view_->more_content_bottom_.get();
  }

 private:
  raw_ptr<ScrollView> scroll_view_;
};

}  // namespace test

namespace {

const int kWidth = 100;
const int kMinHeight = 50;
const int kMaxHeight = 100;

class FixedView : public View {
  METADATA_HEADER(FixedView, View)

 public:
  FixedView() = default;

  FixedView(const FixedView&) = delete;
  FixedView& operator=(const FixedView&) = delete;

  ~FixedView() override = default;

  std::optional<bool> did_show_tooltip_in_viewport() const {
    return did_show_tooltip_in_viewport_;
  }

  void Layout(PassKey) override {
    gfx::Size pref = GetPreferredSize({});
    SetBounds(x(), y(), pref.width(), pref.height());
  }

  void UpdateTooltipForFocus() override {
    View::UpdateTooltipForFocus();

    // To verify `UpdateTooltipForFocus()` is called after the view scrolled, we
    // can set `did_show_tooltip_in_viewport_` as true only when this view is in
    // the visible rect.
    did_show_tooltip_in_viewport_ = !GetVisibleBounds().IsEmpty();
  }

  void SetFocus() { Focus(); }

 private:
  // True if this view is in the viewport of its parent scroll view to show the
  // tooltip.
  std::optional<bool> did_show_tooltip_in_viewport_;
};

BEGIN_METADATA(FixedView)
END_METADATA

class CustomView : public View {
  METADATA_HEADER(CustomView, View)

 public:
  CustomView() = default;

  CustomView(const CustomView&) = delete;
  CustomView& operator=(const CustomView&) = delete;

  ~CustomView() override = default;

  const gfx::Point last_location() const { return last_location_; }

  void Layout(PassKey) override {
    gfx::Size pref = GetPreferredSize({});
    int width = pref.width();
    int height = pref.height();
    if (parent()) {
      width = std::max(parent()->width(), width);
      height = std::max(parent()->height(), height);
    }
    SetBounds(x(), y(), width, height);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    last_location_ = event.location();
    return true;
  }

 private:
  gfx::Point last_location_;
};

BEGIN_METADATA(CustomView)
END_METADATA

void CheckScrollbarVisibility(const ScrollView* scroll_view,
                              ScrollBarOrientation orientation,
                              bool should_be_visible) {
  const ScrollBar* scrollbar = orientation == HORIZONTAL
                                   ? scroll_view->horizontal_scroll_bar()
                                   : scroll_view->vertical_scroll_bar();
  if (should_be_visible) {
    ASSERT_TRUE(scrollbar);
    EXPECT_TRUE(scrollbar->GetVisible());
  } else {
    EXPECT_TRUE(!scrollbar || !scrollbar->GetVisible());
  }
}

ui::MouseEvent TestLeftMouseAt(const gfx::Point& location, ui::EventType type) {
  return ui::MouseEvent(type, location, location, base::TimeTicks(),
                        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
}

// This view has a large width, but the height always matches the parent's
// height. This is similar to a TableView that has many columns showing, but
// very few rows.
class VerticalResizingView : public View {
  METADATA_HEADER(VerticalResizingView, View)

 public:
  VerticalResizingView() = default;

  VerticalResizingView(const VerticalResizingView&) = delete;
  VerticalResizingView& operator=(const VerticalResizingView&) = delete;

  ~VerticalResizingView() override = default;
  void Layout(PassKey) override {
    int width = 10000;
    int height = parent()->height();
    SetBounds(x(), y(), width, height);
  }
};

BEGIN_METADATA(VerticalResizingView)
END_METADATA

// Same as VerticalResizingView, but horizontal instead.
class HorizontalResizingView : public View {
  METADATA_HEADER(HorizontalResizingView, View)

 public:
  HorizontalResizingView() = default;

  HorizontalResizingView(const HorizontalResizingView&) = delete;
  HorizontalResizingView& operator=(const HorizontalResizingView&) = delete;

  ~HorizontalResizingView() override = default;
  void Layout(PassKey) override {
    int height = 10000;
    int width = parent()->width();
    SetBounds(x(), y(), width, height);
  }
};

BEGIN_METADATA(HorizontalResizingView)
END_METADATA

class TestScrollBarThumb : public BaseScrollBarThumb {
  METADATA_HEADER(TestScrollBarThumb, BaseScrollBarThumb)

 public:
  using BaseScrollBarThumb::BaseScrollBarThumb;

  // BaseScrollBarThumb:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override {
    return gfx::Size(1, 1);
  }
  void OnPaint(gfx::Canvas* canvas) override {}
};

BEGIN_METADATA(TestScrollBarThumb)
END_METADATA

class TestScrollBar : public ScrollBar {
  METADATA_HEADER(TestScrollBar, ScrollBar)

 public:
  TestScrollBar(Orientation orientation, bool overlaps_content, int thickness)
      : ScrollBar(orientation),
        overlaps_content_(overlaps_content),
        thickness_(thickness) {
    SetThumb(new TestScrollBarThumb(this));
  }

  // ScrollBar:
  int GetThickness() const override { return thickness_; }
  bool OverlapsContent() const override { return overlaps_content_; }
  gfx::Rect GetTrackBounds() const override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.set_width(GetThickness());
    return bounds;
  }

 private:
  const bool overlaps_content_ = false;
  const int thickness_ = 0;
};

BEGIN_METADATA(TestScrollBar)
END_METADATA

}  // namespace

using test::ScrollViewTestApi;

// Simple test harness for testing a ScrollView directly.
class ScrollViewTest : public ViewsTestBase {
 public:
  ScrollViewTest() = default;

  ScrollViewTest(const ScrollViewTest&) = delete;
  ScrollViewTest& operator=(const ScrollViewTest&) = delete;

  void SetUp() override {
    ViewsTestBase::SetUp();
    scroll_view_ = std::make_unique<ScrollView>();
  }

  View* InstallContents() {
    const gfx::Rect default_outer_bounds(0, 0, 100, 100);
    auto* view = scroll_view_->SetContents(std::make_unique<View>());
    scroll_view_->SetBoundsRect(default_outer_bounds);
    // Setting the contents will invalidate the layout. Call RunScheduledLayout
    // to immediately run the scheduled layout.
    views::test::RunScheduledLayout(scroll_view_.get());
    return view;
  }

  // For the purposes of these tests, we directly call SetBounds on the
  // contents of the ScrollView to see how the ScrollView reacts to it. Calling
  // SetBounds will not invalidate parent layouts. Thus, we need to manually
  // invalidate the layout of the scroll view. Production code should not need
  // to manually invalidate the layout of the ScrollView as the ScrollView's
  // layout should be invalidated through the call paths that change the size of
  // the ScrollView's contents. (For example, via AddChildView).
  void InvalidateAndRunScheduledLayoutOnScrollView() {
    scroll_view_->InvalidateLayout();
    views::test::RunScheduledLayout(scroll_view_.get());
  }

 protected:
#if BUILDFLAG(IS_MAC)
  void SetOverlayScrollersEnabled(bool enabled) {
    // Ensure the old scroller override is destroyed before creating a new one.
    // Otherwise, the swizzlers are interleaved and restore incorrect methods.
    scroller_style_.reset();
    scroller_style_ =
        std::make_unique<ui::test::ScopedPreferredScrollerStyle>(enabled);
  }

 private:
  // Disable overlay scrollers by default. This needs to be set before
  // |scroll_view_| is initialized, otherwise scrollers will try to animate to
  // change modes, which requires a MessageLoop to exist. Tests should only
  // modify this via SetOverlayScrollersEnabled().
  std::unique_ptr<ui::test::ScopedPreferredScrollerStyle> scroller_style_ =
      std::make_unique<ui::test::ScopedPreferredScrollerStyle>(false);

 protected:
#endif
  int VerticalScrollBarWidth() {
    return scroll_view_->vertical_scroll_bar()->GetThickness();
  }

  int HorizontalScrollBarHeight() {
    return scroll_view_->horizontal_scroll_bar()->GetThickness();
  }

  std::unique_ptr<ScrollView> scroll_view_;
};

// Test harness that includes a Widget to help test ui::Event handling.
class WidgetScrollViewTest : public test::WidgetTest,
                             public ui::CompositorObserver {
 public:
  static constexpr int kDefaultHeight = 100;
  static constexpr int kDefaultWidth = 100;

  WidgetScrollViewTest() = default;

  WidgetScrollViewTest(const WidgetScrollViewTest&) = delete;
  WidgetScrollViewTest& operator=(const WidgetScrollViewTest&) = delete;

  // Call this before adding the ScrollView to test with overlay scrollbars.
  void SetUseOverlayScrollers() { use_overlay_scrollers_ = true; }

  // Adds a ScrollView with the given |contents_view| and does layout.
  ScrollView* AddScrollViewWithContents(std::unique_ptr<View> contents,
                                        bool commit_layers = true) {
#if BUILDFLAG(IS_MAC)
    scroller_style_ = std::make_unique<ui::test::ScopedPreferredScrollerStyle>(
        use_overlay_scrollers_);
#endif

    const gfx::Rect default_bounds(50, 50, kDefaultWidth, kDefaultHeight);
    widget_ = CreateTopLevelFramelessPlatformWidget();
    widget_->SetBounds(default_bounds);
    widget_->Show();

    ScrollView* scroll_view =
        widget_->SetContentsView(std::make_unique<ScrollView>());
    scroll_view->SetContents(std::move(contents));
    views::test::RunScheduledLayout(scroll_view);

    widget_->GetCompositor()->AddObserver(this);

    // Ensure the Compositor has committed layer changes before attempting to
    // use them for impl-side scrolling. Note that simply RunUntilIdle() works
    // when tests are run in isolation, but compositor scheduling can interact
    // between test runs in the general case.
    if (commit_layers)
      WaitForCommit();
    return scroll_view;
  }

  // Adds a ScrollView with a contents view of the given |size| and does layout.
  ScrollView* AddScrollViewWithContentSize(const gfx::Size& contents_size,
                                           bool commit_layers = true) {
    auto contents = std::make_unique<View>();
    contents->SetSize(contents_size);
    return AddScrollViewWithContents(std::move(contents), commit_layers);
  }

  // Wait for a commit to be observed on the compositor.
  void WaitForCommit() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, quit_closure_, TestTimeouts::action_timeout());
    run_loop.Run();
    EXPECT_TRUE(quit_closure_.is_null()) << "Timed out waiting for a commit.";
  }

  void TestClickAt(const gfx::Point& location) {
    ui::MouseEvent press(
        TestLeftMouseAt(location, ui::EventType::kMousePressed));
    ui::MouseEvent release(
        TestLeftMouseAt(location, ui::EventType::kMouseReleased));
    widget_->OnMouseEvent(&press);
    widget_->OnMouseEvent(&release);
  }

  // testing::Test:
  void TearDown() override {
    widget_->GetCompositor()->RemoveObserver(this);
    if (widget_) {
      widget_.ExtractAsDangling()->CloseNow();
    }
    WidgetTest::TearDown();
  }

 protected:
  Widget* widget() const { return widget_; }

 private:
  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override {
    quit_closure_.Run();
    quit_closure_.Reset();
  }

  raw_ptr<Widget> widget_ = nullptr;

  // Disable scrollbar hiding (i.e. disable overlay scrollbars) by default.
  bool use_overlay_scrollers_ = false;

  base::RepeatingClosure quit_closure_;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<ui::test::ScopedPreferredScrollerStyle> scroller_style_;
#endif
};

constexpr int WidgetScrollViewTest::kDefaultHeight;
constexpr int WidgetScrollViewTest::kDefaultWidth;

// A gtest parameter to permute over whether ScrollView uses a left-to-right or
// right-to-left locale, or whether it uses ui::Layers or View bounds offsets to
// position contents (i.e. ::features::kUiCompositorScrollWithLayers).
enum class UiConfig { kLtr, kLtrWithLayers, kRtl, kRtlWithLayers };

class WidgetScrollViewTestRTLAndLayers
    : public WidgetScrollViewTest,
      public ::testing::WithParamInterface<UiConfig> {
 public:
  WidgetScrollViewTestRTLAndLayers() : locale_(IsTestingRtl() ? "he" : "en") {
    if (IsTestingLayers()) {
      layer_config_.InitAndEnableFeature(
          ::features::kUiCompositorScrollWithLayers);
    } else {
      layer_config_.InitAndDisableFeature(
          ::features::kUiCompositorScrollWithLayers);
    }
  }

  WidgetScrollViewTestRTLAndLayers(const WidgetScrollViewTestRTLAndLayers&) =
      delete;
  WidgetScrollViewTestRTLAndLayers& operator=(
      const WidgetScrollViewTestRTLAndLayers&) = delete;

  bool IsTestingRtl() const {
    return GetParam() == UiConfig::kRtl ||
           GetParam() == UiConfig::kRtlWithLayers;
  }

  bool IsTestingLayers() const {
    return GetParam() == UiConfig::kLtrWithLayers ||
           GetParam() == UiConfig::kRtlWithLayers;
  }

  // Returns a point in the coordinate space of |target| by translating a point
  // inset one pixel from the top of the Widget and one pixel on the leading
  // side of the Widget. There should be no scroll bar on this side. If
  // |flip_result| is true, automatically account for flipped coordinates in
  // RTL.
  gfx::Point HitTestInCorner(View* target, bool flip_result = true) const {
    const gfx::Point test_mouse_point_in_root =
        IsTestingRtl() ? gfx::Point(kDefaultWidth - 1, 1) : gfx::Point(1, 1);
    gfx::Point point = test_mouse_point_in_root;
    View::ConvertPointToTarget(widget()->GetRootView(), target, &point);
    if (flip_result)
      return gfx::Point(target->GetMirroredXInView(point.x()), point.y());
    return point;
  }

 private:
  base::test::ScopedRestoreICUDefaultLocale locale_;
  base::test::ScopedFeatureList layer_config_;
};

std::string UiConfigToString(const testing::TestParamInfo<UiConfig>& info) {
  switch (info.param) {
    case UiConfig::kLtr:
      return "LTR";
    case UiConfig::kLtrWithLayers:
      return "LTR_LAYERS";
    case UiConfig::kRtl:
      return "RTL";
    case UiConfig::kRtlWithLayers:
      return "RTL_LAYERS";
  }
  NOTREACHED();
}

// Verifies the viewport is sized to fit the available space.
TEST_F(ScrollViewTest, ViewportSizedToFit) {
  View* contents = InstallContents();
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the viewport and content is sized to fit the available space for
// bounded scroll view.
TEST_F(ScrollViewTest, BoundedViewportSizedToFit) {
  View* contents = InstallContents();
  scroll_view_->ClipHeightTo(100, 200);
  scroll_view_->SetBorder(CreateSolidBorder(2, 0));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ("2,2 96x96", contents->parent()->bounds().ToString());

  // Make sure the width of |contents| is set properly not to overflow the
  // viewport.
  EXPECT_EQ(96, contents->width());
}

// Verifies that the vertical scrollbar does not unnecessarily appear for a
// contents whose height always matches the height of the viewport.
TEST_F(ScrollViewTest, VerticalScrollbarDoesNotAppearUnnecessarily) {
  const gfx::Rect default_outer_bounds(0, 0, 100, 100);
  scroll_view_->SetContents(std::make_unique<VerticalResizingView>());
  scroll_view_->SetBoundsRect(default_outer_bounds);
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_FALSE(scroll_view_->vertical_scroll_bar()->GetVisible());
  EXPECT_TRUE(scroll_view_->horizontal_scroll_bar()->GetVisible());
}

// Same as above, but setting horizontal scroll bar to hidden.
TEST_F(ScrollViewTest, HorizontalScrollbarDoesNotAppearIfHidden) {
  const gfx::Rect default_outer_bounds(0, 0, 100, 100);
  scroll_view_->SetHorizontalScrollBarMode(
      ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetContents(std::make_unique<VerticalResizingView>());
  scroll_view_->SetBoundsRect(default_outer_bounds);
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_FALSE(scroll_view_->vertical_scroll_bar()->GetVisible());
  EXPECT_FALSE(scroll_view_->horizontal_scroll_bar()->GetVisible());
}

// Same as above, but setting vertical scrollbar instead.
TEST_F(ScrollViewTest, VerticalScrollbarDoesNotAppearIfHidden) {
  const gfx::Rect default_outer_bounds(0, 0, 100, 100);
  scroll_view_->SetVerticalScrollBarMode(
      ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetContents(std::make_unique<HorizontalResizingView>());
  scroll_view_->SetBoundsRect(default_outer_bounds);
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_FALSE(scroll_view_->vertical_scroll_bar()->GetVisible());
  EXPECT_FALSE(scroll_view_->horizontal_scroll_bar()->GetVisible());
}

// Same as above, but setting horizontal scroll bar to disabled.
TEST_F(ScrollViewTest, HorizontalScrollbarDoesNotAppearIfDisabled) {
  const gfx::Rect default_outer_bounds(0, 0, 100, 100);
  scroll_view_->SetHorizontalScrollBarMode(
      ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->SetContents(std::make_unique<VerticalResizingView>());
  scroll_view_->SetBoundsRect(default_outer_bounds);
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_FALSE(scroll_view_->vertical_scroll_bar()->GetVisible());
  EXPECT_FALSE(scroll_view_->horizontal_scroll_bar()->GetVisible());
}

// Same as above, but setting vertical scrollbar instead.
TEST_F(ScrollViewTest, VerticallScrollbarDoesNotAppearIfDisabled) {
  const gfx::Rect default_outer_bounds(0, 0, 100, 100);
  scroll_view_->SetVerticalScrollBarMode(ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->SetContents(std::make_unique<HorizontalResizingView>());
  scroll_view_->SetBoundsRect(default_outer_bounds);
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_FALSE(scroll_view_->vertical_scroll_bar()->GetVisible());
  EXPECT_FALSE(scroll_view_->horizontal_scroll_bar()->GetVisible());
}

TEST_F(ScrollViewTest, AccessibleProperties) {
  ScrollViewTestApi test_api(scroll_view_.get());
  InstallContents();

  ui::AXNodeData data;
  scroll_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kScrollView);
}

// Verifies the scrollbars are added as necessary.
// If on Mac, test the non-overlay scrollbars.
TEST_F(ScrollViewTest, ScrollBars) {
  View* contents = InstallContents();

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth(),
            contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, false);
  EXPECT_TRUE(!scroll_view_->horizontal_scroll_bar() ||
              !scroll_view_->horizontal_scroll_bar()->GetVisible());
  ASSERT_TRUE(scroll_view_->vertical_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->vertical_scroll_bar()->GetVisible());

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight(),
            contents->parent()->height());
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, false);
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, true);

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth(),
            contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight(),
            contents->parent()->height());
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, true);

  // Add a border, test vertical scrollbar.
  const int kTopPadding = 1;
  const int kLeftPadding = 2;
  const int kBottomPadding = 3;
  const int kRightPadding = 4;
  scroll_view_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(
      kTopPadding, kLeftPadding, kBottomPadding, kRightPadding)));
  contents->SetBounds(0, 0, 50, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth() - kLeftPadding -
                kRightPadding,
            contents->parent()->width());
  EXPECT_EQ(100 - kTopPadding - kBottomPadding, contents->parent()->height());
  EXPECT_TRUE(!scroll_view_->horizontal_scroll_bar() ||
              !scroll_view_->horizontal_scroll_bar()->GetVisible());
  ASSERT_TRUE(scroll_view_->vertical_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->vertical_scroll_bar()->GetVisible());
  gfx::Rect bounds = scroll_view_->vertical_scroll_bar()->bounds();
  EXPECT_EQ(100 - VerticalScrollBarWidth() - kRightPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(kTopPadding, bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());

  // Horizontal with border.
  contents->SetBounds(0, 0, 400, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100 - kLeftPadding - kRightPadding, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight() - kTopPadding -
                kBottomPadding,
            contents->parent()->height());
  ASSERT_TRUE(scroll_view_->horizontal_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->horizontal_scroll_bar()->GetVisible());
  EXPECT_TRUE(!scroll_view_->vertical_scroll_bar() ||
              !scroll_view_->vertical_scroll_bar()->GetVisible());
  bounds = scroll_view_->horizontal_scroll_bar()->bounds();
  EXPECT_EQ(kLeftPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(100 - kBottomPadding - HorizontalScrollBarHeight(), bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());

  // Both horizontal and vertical with border.
  contents->SetBounds(0, 0, 300, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth() - kLeftPadding -
                kRightPadding,
            contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight() - kTopPadding -
                kBottomPadding,
            contents->parent()->height());
  bounds = scroll_view_->horizontal_scroll_bar()->bounds();
  // Check horiz.
  ASSERT_TRUE(scroll_view_->horizontal_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->horizontal_scroll_bar()->GetVisible());
  bounds = scroll_view_->horizontal_scroll_bar()->bounds();
  EXPECT_EQ(kLeftPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding - VerticalScrollBarWidth(), bounds.right());
  EXPECT_EQ(100 - kBottomPadding - HorizontalScrollBarHeight(), bounds.y());
  EXPECT_EQ(100 - kBottomPadding, bounds.bottom());
  // Check vert.
  ASSERT_TRUE(scroll_view_->vertical_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->vertical_scroll_bar()->GetVisible());
  bounds = scroll_view_->vertical_scroll_bar()->bounds();
  EXPECT_EQ(100 - VerticalScrollBarWidth() - kRightPadding, bounds.x());
  EXPECT_EQ(100 - kRightPadding, bounds.right());
  EXPECT_EQ(kTopPadding, bounds.y());
  EXPECT_EQ(100 - kBottomPadding - HorizontalScrollBarHeight(),
            bounds.bottom());
}

// Tests that after scrolling the child (which was gained the focus) into the
// viewport of the scroll view, the tooltip should be shown up.
TEST_F(WidgetScrollViewTest, ScrollChildToVisibleOnFocusWithTooltip) {
  // Create a scroll view and its contents view.
  auto contents_ptr = std::make_unique<CustomView>();
  contents_ptr->SetPreferredSize(gfx::Size(100, 200));
  auto* contents = contents_ptr.get();
  AddScrollViewWithContents(std::move(contents_ptr));

  // Create a child view into the contents view, and set it position outside of
  // the visible rect.
  auto child = std::make_unique<FixedView>();
  child->SetPreferredSize(gfx::Size(10, 10));
  child->SetPosition(gfx::Point(50, 110));
  auto* child_ptr = contents->AddChildView(std::move(child));

  // Bring a focus to this child view which should scroll it into the visible
  // rect of the scroll view.
  child_ptr->SetFocus();

  ASSERT_TRUE(child_ptr->did_show_tooltip_in_viewport().has_value());
  EXPECT_TRUE(child_ptr->did_show_tooltip_in_viewport().value());
}

// Assertions around adding a header.
TEST_F(WidgetScrollViewTest, Header) {
  auto contents_ptr = std::make_unique<View>();
  auto* contents = contents_ptr.get();
  ScrollView* scroll_view = AddScrollViewWithContents(std::move(contents_ptr));
  auto* header = scroll_view->SetHeader(std::make_unique<CustomView>());
  View* header_parent = header->parent();

  views::test::RunScheduledLayout(widget());
  // |header|s preferred size is empty, which should result in all space going
  // to contents.
  EXPECT_EQ("0,0 100x0", header->parent()->bounds().ToString());
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());

  // With layered scrolling, ScrollView::Layout() will impose a size on the
  // contents that fills the viewport. Since the test view doesn't have its own
  // Layout, reset it in this case so that adding a header doesn't shift the
  // contents down and require scrollbars.
  if (contents->layer()) {
    EXPECT_EQ("0,0 100x100", contents->bounds().ToString());
    contents->SetBoundsRect(gfx::Rect());
  }
  EXPECT_EQ("0,0 0x0", contents->bounds().ToString());

  // Get the header a height of 20.
  header->SetPreferredSize(gfx::Size(10, 20));
  EXPECT_TRUE(ViewTestApi(scroll_view).needs_layout());
  views::test::RunScheduledLayout(widget());
  EXPECT_EQ("0,0 100x20", header->parent()->bounds().ToString());
  EXPECT_EQ("0,20 100x80", contents->parent()->bounds().ToString());
  if (contents->layer()) {
    EXPECT_EQ("0,0 100x80", contents->bounds().ToString());
    contents->SetBoundsRect(gfx::Rect());
  }
  EXPECT_EQ("0,0 0x0", contents->bounds().ToString());

  // Remove the header.
  scroll_view->SetHeader(nullptr);
  // SetHeader(nullptr) deletes header.
  header = nullptr;
  views::test::RunScheduledLayout(widget());
  EXPECT_EQ("0,0 100x0", header_parent->bounds().ToString());
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the scrollbars are added as necessary when a header is present.
TEST_F(ScrollViewTest, ScrollBarsWithHeader) {
  auto* header = scroll_view_->SetHeader(std::make_unique<CustomView>());
  View* contents = InstallContents();

  header->SetPreferredSize(gfx::Size(10, 20));

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth(),
            contents->parent()->width());
  EXPECT_EQ(80, contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth(),
            header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  EXPECT_TRUE(!scroll_view_->horizontal_scroll_bar() ||
              !scroll_view_->horizontal_scroll_bar()->GetVisible());
  ASSERT_TRUE(scroll_view_->vertical_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->vertical_scroll_bar()->GetVisible());
  // Make sure the vertical scrollbar overlaps the header for traditional
  // scrollbars and doesn't overlap the header for overlay scrollbars.
  const int expected_scrollbar_y =
      scroll_view_->vertical_scroll_bar()->OverlapsContent()
          ? header->bounds().bottom()
          : header->y();
  EXPECT_EQ(expected_scrollbar_y, scroll_view_->vertical_scroll_bar()->y());
  EXPECT_EQ(header->y(), contents->y());

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight() - 20,
            contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100, header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  ASSERT_TRUE(scroll_view_->horizontal_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->horizontal_scroll_bar()->GetVisible());
  EXPECT_TRUE(!scroll_view_->vertical_scroll_bar() ||
              !scroll_view_->vertical_scroll_bar()->GetVisible());

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth(),
            contents->parent()->width());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight() - 20,
            contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth(),
            header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  ASSERT_TRUE(scroll_view_->horizontal_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->horizontal_scroll_bar()->GetVisible());
  ASSERT_TRUE(scroll_view_->vertical_scroll_bar() != nullptr);
  EXPECT_TRUE(scroll_view_->vertical_scroll_bar()->GetVisible());
}

// Verifies the header scrolls horizontally with the content.
TEST_F(ScrollViewTest, HeaderScrollsWithContent) {
  ScrollViewTestApi test_api(scroll_view_.get());
  auto contents = std::make_unique<CustomView>();
  contents->SetPreferredSize(gfx::Size(500, 500));
  scroll_view_->SetContents(std::move(contents));

  auto* header = scroll_view_->SetHeader(std::make_unique<CustomView>());
  header->SetPreferredSize(gfx::Size(500, 20));

  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(gfx::Vector2d(0, 0), test_api.IntegralViewOffset());
  EXPECT_EQ(gfx::Point(0, 0), header->origin());

  // Scroll the horizontal scrollbar.
  ASSERT_TRUE(scroll_view_->horizontal_scroll_bar());
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(HORIZONTAL), 1);
  EXPECT_EQ(gfx::Vector2d(-1, 0), test_api.IntegralViewOffset());
  EXPECT_EQ(gfx::Point(-1, 0), header->origin());

  // Scrolling the vertical scrollbar shouldn't effect the header.
  ASSERT_TRUE(scroll_view_->vertical_scroll_bar());
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), 1);
  EXPECT_EQ(gfx::Vector2d(-1, -1), test_api.IntegralViewOffset());
  EXPECT_EQ(gfx::Point(-1, 0), header->origin());
}

// Test that calling ScrollToPosition() also updates the position of the
// corresponding ScrollBar.
TEST_F(ScrollViewTest, ScrollToPositionUpdatesScrollBar) {
  ScrollViewTestApi test_api(scroll_view_.get());
  View* contents = InstallContents();

  // Scroll the horizontal scrollbar, after which, the scroll bar thumb position
  // should be updated (i.e. it should be non-zero).
  contents->SetBounds(0, 0, 400, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  auto* scroll_bar = test_api.GetScrollBar(HORIZONTAL);
  ASSERT_TRUE(scroll_bar);
  EXPECT_TRUE(scroll_bar->GetVisible());
  EXPECT_EQ(0, scroll_bar->GetPosition());
  scroll_view_->ScrollToPosition(scroll_bar, 20);
  EXPECT_GT(scroll_bar->GetPosition(), 0);

  // Scroll the vertical scrollbar.
  contents->SetBounds(0, 0, 50, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  scroll_bar = test_api.GetScrollBar(VERTICAL);
  ASSERT_TRUE(scroll_bar);
  EXPECT_TRUE(scroll_bar->GetVisible());
  EXPECT_EQ(0, scroll_bar->GetPosition());
  scroll_view_->ScrollToPosition(scroll_bar, 20);
  EXPECT_GT(scroll_bar->GetPosition(), 0);
}

TEST_F(ScrollViewTest, HorizontalScrollBarAccessibleScrollXProperties) {
  ScrollViewTestApi test_api(scroll_view_.get());
  View* contents = InstallContents();
  contents->SetBounds(0, 0, 500, 20);
  InvalidateAndRunScheduledLayoutOnScrollView();
  auto* scroll_bar = test_api.GetScrollBar(HORIZONTAL);
  ASSERT_TRUE(scroll_bar);

  // Verify kScrollXMin and kScrollXMax with initial content size.
  ui::AXNodeData scroll_view_node_data;
  scroll_view_node_data = ui::AXNodeData();
  scroll_view_->GetViewAccessibility().GetAccessibleNodeData(
      &scroll_view_node_data);
  EXPECT_TRUE(scroll_view_node_data.GetBoolAttribute(
      ax::mojom::BoolAttribute::kScrollable));
  EXPECT_EQ(scroll_bar->GetMinPosition(),
            scroll_view_node_data.GetIntAttribute(
                ax::mojom::IntAttribute::kScrollXMin));
  EXPECT_EQ(scroll_bar->GetMaxPosition(),
            scroll_view_node_data.GetIntAttribute(
                ax::mojom::IntAttribute::kScrollXMax));

  // Set the content width to 400 after which kScrollXMax should be updated
  // and kScrollXMin remains 0 as GetMinPosition() always returns 0.
  contents->SetBounds(0, 0, 400, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  scroll_view_node_data = ui::AXNodeData();
  scroll_view_->GetViewAccessibility().GetAccessibleNodeData(
      &scroll_view_node_data);
  EXPECT_EQ(0, scroll_view_node_data.GetIntAttribute(
                   ax::mojom::IntAttribute::kScrollXMin));
  EXPECT_EQ(400 - test_api.contents_viewport()->width(),
            scroll_view_node_data.GetIntAttribute(
                ax::mojom::IntAttribute::kScrollXMax));

  // Scroll the horizontal scrollbar after which kScrollX should be updated.
  EXPECT_EQ(0, scroll_view_node_data.GetIntAttribute(
                   ax::mojom::IntAttribute::kScrollX));
  scroll_view_->ScrollToPosition(scroll_bar, 20);
  scroll_view_node_data = ui::AXNodeData();
  scroll_view_->GetViewAccessibility().GetAccessibleNodeData(
      &scroll_view_node_data);
  EXPECT_EQ(
      test_api.CurrentOffset().x(),
      scroll_view_node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollX));
}

TEST_F(ScrollViewTest, VerticalScrollBarAccessibleScrollYProperties) {
  ScrollViewTestApi test_api(scroll_view_.get());
  View* contents = InstallContents();
  contents->SetBounds(0, 0, 50, 500);
  InvalidateAndRunScheduledLayoutOnScrollView();
  auto* scroll_bar = test_api.GetScrollBar(VERTICAL);
  ASSERT_TRUE(scroll_bar);

  // Verify kScrollYMin and kScrollYMax with initial content size.
  ui::AXNodeData scroll_view_node_data;
  scroll_view_node_data = ui::AXNodeData();
  scroll_view_->GetViewAccessibility().GetAccessibleNodeData(
      &scroll_view_node_data);
  EXPECT_TRUE(scroll_view_node_data.GetBoolAttribute(
      ax::mojom::BoolAttribute::kScrollable));
  EXPECT_EQ(scroll_bar->GetMinPosition(),
            scroll_view_node_data.GetIntAttribute(
                ax::mojom::IntAttribute::kScrollYMin));
  EXPECT_EQ(scroll_bar->GetMaxPosition(),
            scroll_view_node_data.GetIntAttribute(
                ax::mojom::IntAttribute::kScrollYMax));

  // Set the content width to 400 after which kScrollYMax should be updated
  // and kScrollYMin remains 0 as GetMinPosition() always returns 0.
  contents->SetBounds(0, 0, 50, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  scroll_view_node_data = ui::AXNodeData();
  scroll_view_->GetViewAccessibility().GetAccessibleNodeData(
      &scroll_view_node_data);
  EXPECT_EQ(0, scroll_view_node_data.GetIntAttribute(
                   ax::mojom::IntAttribute::kScrollYMin));
  EXPECT_EQ(400 - test_api.contents_viewport()->height(),
            scroll_view_node_data.GetIntAttribute(
                ax::mojom::IntAttribute::kScrollYMax));

  // Scroll the vertical scrollbar after which kScrollY should be updated.
  EXPECT_EQ(0, scroll_view_node_data.GetIntAttribute(
                   ax::mojom::IntAttribute::kScrollY));
  scroll_view_->ScrollToPosition(scroll_bar, 20);
  scroll_view_node_data = ui::AXNodeData();
  scroll_view_->GetViewAccessibility().GetAccessibleNodeData(
      &scroll_view_node_data);
  EXPECT_EQ(
      test_api.CurrentOffset().y(),
      scroll_view_node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollY));
}

// Test that calling ScrollToPosition() also updates the position of the
// child view even when the horizontal scrollbar is hidden.
TEST_F(ScrollViewTest, ScrollToPositionUpdatesWithHiddenHorizontalScrollBar) {
  scroll_view_->SetHorizontalScrollBarMode(
      ScrollView::ScrollBarMode::kHiddenButEnabled);
  ScrollViewTestApi test_api(scroll_view_.get());
  View* contents = InstallContents();

  contents->SetBounds(0, 0, 400, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  auto* scroll_bar = test_api.GetScrollBar(HORIZONTAL);
  ASSERT_TRUE(scroll_bar);
  EXPECT_FALSE(scroll_bar->GetVisible());
  // We can't rely on the scrollbar, which may not be updated as it's not
  // visible, but we can check the scroll offset itself.
  EXPECT_EQ(0, test_api.CurrentOffset().x());
  scroll_view_->ScrollToPosition(scroll_bar, 20);
  EXPECT_GT(test_api.CurrentOffset().x(), 0);
}

// Test that calling ScrollToPosition() also updates the position of the
// child view even when the horizontal scrollbar is hidden.
TEST_F(ScrollViewTest, ScrollToPositionUpdatesWithHiddenVerticalScrollBar) {
  scroll_view_->SetVerticalScrollBarMode(
      ScrollView::ScrollBarMode::kHiddenButEnabled);
  ScrollViewTestApi test_api(scroll_view_.get());
  View* contents = InstallContents();

  contents->SetBounds(0, 0, 50, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  auto* scroll_bar = test_api.GetScrollBar(VERTICAL);
  ASSERT_TRUE(scroll_bar);
  EXPECT_FALSE(scroll_bar->GetVisible());
  // We can't rely on the scrollbar, which may not be updated as it's not
  // visible, but we can check the scroll offset itself.
  EXPECT_EQ(0, test_api.CurrentOffset().y());
  scroll_view_->ScrollToPosition(scroll_bar, 20);
  EXPECT_GT(test_api.CurrentOffset().y(), 0);
}

// Verifies ScrollRectToVisible() on the child works.
TEST_F(ScrollViewTest, ScrollRectToVisible) {
  ScrollViewTestApi test_api(scroll_view_.get());
  auto contents = std::make_unique<CustomView>();
  contents->SetPreferredSize(gfx::Size(500, 1000));
  auto* contents_ptr = scroll_view_->SetContents(std::move(contents));

  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(gfx::Vector2d(0, 0), test_api.IntegralViewOffset());

  // Scroll to y=405 height=10, this should make the y position of the content
  // at (405 + 10) - viewport_height (scroll region bottom aligned).
  contents_ptr->ScrollRectToVisible(gfx::Rect(0, 405, 10, 10));
  const int viewport_height = test_api.contents_viewport()->height();

  // Expect there to be a horizontal scrollbar, making the viewport shorter.
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight(), viewport_height);

  gfx::PointF offset = test_api.CurrentOffset();
  EXPECT_EQ(415 - viewport_height, offset.y());

  // Scroll to the current y-location and 10x10; should do nothing.
  contents_ptr->ScrollRectToVisible(gfx::Rect(0, offset.y(), 10, 10));
  EXPECT_EQ(415 - viewport_height, test_api.CurrentOffset().y());
}

// Verifies ScrollByOffset() method works as expected
TEST_F(ScrollViewTest, ScrollByOffset) {
  // setup
  ScrollViewTestApi test_api(scroll_view_.get());
  auto contents = std::make_unique<CustomView>();
  contents->SetPreferredSize(gfx::Size(500, 1000));
  scroll_view_->SetContents(std::move(contents));
  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(gfx::Vector2d(0, 0), test_api.IntegralViewOffset());

  // scroll by an offset of x=5 and y=5
  scroll_view_->ScrollByOffset(gfx::PointF(5, 5));

  EXPECT_EQ(test_api.CurrentOffset().x(), 5);
  EXPECT_EQ(test_api.CurrentOffset().y(), 5);

  // scroll back to the initial position
  scroll_view_->ScrollByOffset(gfx::PointF(-5, -5));

  EXPECT_EQ(test_api.CurrentOffset().x(), 0);
  EXPECT_EQ(test_api.CurrentOffset().y(), 0);
}

// Verifies ScrollRectToVisible() scrolls the view horizontally even if the
// horizontal scrollbar is hidden (but not disabled).
TEST_F(ScrollViewTest, ScrollRectToVisibleWithHiddenHorizontalScrollbar) {
  scroll_view_->SetHorizontalScrollBarMode(
      ScrollView::ScrollBarMode::kHiddenButEnabled);
  ScrollViewTestApi test_api(scroll_view_.get());
  auto contents = std::make_unique<CustomView>();
  contents->SetPreferredSize(gfx::Size(500, 1000));
  auto* contents_ptr = scroll_view_->SetContents(std::move(contents));

  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(gfx::Vector2d(0, 0), test_api.IntegralViewOffset());

  // Scroll to x=305 width=10, this should make the x position of the content
  // at (305 + 10) - viewport_width (scroll region right aligned).
  contents_ptr->ScrollRectToVisible(gfx::Rect(305, 0, 10, 10));
  const int viewport_width = test_api.contents_viewport()->width();

  // Expect there to be a vertical scrollbar, making the viewport shorter.
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutWidth(), viewport_width);

  gfx::PointF offset = test_api.CurrentOffset();
  EXPECT_EQ(315 - viewport_width, offset.x());

  // Scroll to the current x-location and 10x10; should do nothing.
  contents_ptr->ScrollRectToVisible(gfx::Rect(offset.x(), 0, 10, 10));
  EXPECT_EQ(315 - viewport_width, test_api.CurrentOffset().x());
}

// Verifies ScrollRectToVisible() scrolls the view vertically even if the
// vertical scrollbar is hidden (but not disabled).
TEST_F(ScrollViewTest, ScrollRectToVisibleWithHiddenVerticalScrollbar) {
  scroll_view_->SetVerticalScrollBarMode(
      ScrollView::ScrollBarMode::kHiddenButEnabled);
  ScrollViewTestApi test_api(scroll_view_.get());
  auto contents = std::make_unique<CustomView>();
  contents->SetPreferredSize(gfx::Size(1000, 500));
  auto* contents_ptr = scroll_view_->SetContents(std::move(contents));

  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(gfx::Vector2d(0, 0), test_api.IntegralViewOffset());

  // Scroll to y=305 height=10, this should make the y position of the content
  // at (305 + 10) - viewport_height (scroll region bottom aligned).
  contents_ptr->ScrollRectToVisible(gfx::Rect(0, 305, 10, 10));
  const int viewport_height = test_api.contents_viewport()->height();

  // Expect there to be a vertical scrollbar, making the viewport shorter.
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight(), viewport_height);

  gfx::PointF offset = test_api.CurrentOffset();
  EXPECT_EQ(315 - viewport_height, offset.y());

  // Scroll to the current y-location and 10x10; should do nothing.
  contents_ptr->ScrollRectToVisible(gfx::Rect(0, offset.y(), 10, 10));
  EXPECT_EQ(315 - viewport_height, test_api.CurrentOffset().y());
}

// Verifies ScrollRectToVisible() does not scroll the view horizontally or
// vertically if the scrollbars are disabled.
TEST_F(ScrollViewTest, ScrollRectToVisibleWithDisabledScrollbars) {
  scroll_view_->SetHorizontalScrollBarMode(
      ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->SetVerticalScrollBarMode(ScrollView::ScrollBarMode::kDisabled);
  ScrollViewTestApi test_api(scroll_view_.get());
  auto contents = std::make_unique<CustomView>();
  contents->SetPreferredSize(gfx::Size(500, 1000));
  auto* contents_ptr = scroll_view_->SetContents(std::move(contents));

  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(gfx::Vector2d(0, 0), test_api.IntegralViewOffset());

  contents_ptr->ScrollRectToVisible(gfx::Rect(305, 0, 10, 10));
  EXPECT_EQ(0, test_api.CurrentOffset().x());

  contents_ptr->ScrollRectToVisible(gfx::Rect(0, 305, 10, 10));
  EXPECT_EQ(0, test_api.CurrentOffset().y());
}

// Verifies that child scrolls into view when it's focused.
TEST_F(ScrollViewTest, ScrollChildToVisibleOnFocus) {
  ScrollViewTestApi test_api(scroll_view_.get());
  auto contents = std::make_unique<CustomView>();
  contents->SetPreferredSize(gfx::Size(500, 1000));
  auto* contents_ptr = scroll_view_->SetContents(std::move(contents));
  auto child = std::make_unique<FixedView>();
  child->SetPreferredSize(gfx::Size(10, 10));
  child->SetPosition(gfx::Point(0, 405));
  auto* child_ptr = contents_ptr->AddChildView(std::move(child));

  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(gfx::Vector2d(), test_api.IntegralViewOffset());

  // Set focus to the child control. This should cause the control to scroll to
  // y=405 height=10. Like the above test, this should make the y position of
  // the content at (405 + 10) - viewport_height (scroll region bottom aligned).
  child_ptr->SetFocus();
  const int viewport_height = test_api.contents_viewport()->height();

  // Expect there to be a horizontal scrollbar, making the viewport shorter.
  EXPECT_EQ(100 - scroll_view_->GetScrollBarLayoutHeight(), viewport_height);

  gfx::PointF offset = test_api.CurrentOffset();
  EXPECT_EQ(415 - viewport_height, offset.y());
}

// Verifies that ScrollView scrolls into view when its contents root is focused.
TEST_F(ScrollViewTest, ScrollViewToVisibleOnContentsRootFocus) {
  ScrollViewTestApi outer_test_api(scroll_view_.get());
  auto outer_contents = std::make_unique<CustomView>();
  outer_contents->SetPreferredSize(gfx::Size(500, 1000));
  auto* outer_contents_ptr =
      scroll_view_->SetContents(std::move(outer_contents));

  auto inner_scroll_view = std::make_unique<ScrollView>();
  auto* inner_scroll_view_ptr =
      outer_contents_ptr->AddChildView(std::move(inner_scroll_view));

  ScrollViewTestApi inner_test_api(inner_scroll_view_ptr);
  auto inner_contents = std::make_unique<FixedView>();
  inner_contents->SetPreferredSize(gfx::Size(500, 1000));
  auto* inner_contents_ptr =
      inner_scroll_view_ptr->SetContents(std::move(inner_contents));

  inner_scroll_view_ptr->SetBoundsRect(gfx::Rect(0, 510, 100, 100));
  views::test::RunScheduledLayout(inner_scroll_view_ptr);
  EXPECT_EQ(gfx::Vector2d(), inner_test_api.IntegralViewOffset());

  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 200, 200));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(gfx::Vector2d(), outer_test_api.IntegralViewOffset());

  // Scroll the inner scroll view to y=405 height=10. This should make the y
  // position of the inner content at (405 + 10) - inner_viewport_height
  // (scroll region bottom aligned). The outer scroll view should not scroll.
  inner_contents_ptr->ScrollRectToVisible(gfx::Rect(0, 405, 10, 10));
  const int inner_viewport_height =
      inner_test_api.contents_viewport()->height();
  gfx::PointF inner_offset = inner_test_api.CurrentOffset();
  EXPECT_EQ(415 - inner_viewport_height, inner_offset.y());
  gfx::PointF outer_offset = outer_test_api.CurrentOffset();
  EXPECT_EQ(0, outer_offset.y());

  // Set focus to the inner scroll view's contents root. This should cause the
  // outer scroll view to scroll to y=510 height=100 so that the y position of
  // the outer content is at (510 + 100) - outer_viewport_height (scroll region
  // bottom aligned). The inner scroll view should not scroll.
  inner_contents_ptr->SetFocus();
  const int outer_viewport_height =
      outer_test_api.contents_viewport()->height();
  inner_offset = inner_test_api.CurrentOffset();
  EXPECT_EQ(415 - inner_viewport_height, inner_offset.y());
  outer_offset = outer_test_api.CurrentOffset();
  EXPECT_EQ(610 - outer_viewport_height, outer_offset.y());
}

// Verifies ClipHeightTo() uses the height of the content when it is between the
// minimum and maximum height values.
TEST_F(ScrollViewTest, ClipHeightToNormalContentHeight) {
  scroll_view_->ClipHeightTo(kMinHeight, kMaxHeight);

  const int kNormalContentHeight = 75;
  scroll_view_->SetContents(std::make_unique<views::StaticSizedView>(
      gfx::Size(kWidth, kNormalContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight),
            scroll_view_->GetPreferredSize({}));

  scroll_view_->SizeToPreferredSize();
  views::test::RunScheduledLayout(scroll_view_.get());

  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight),
            scroll_view_->contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight), scroll_view_->size());
}

// Verifies ClipHeightTo() uses the minimum height when the content is shorter
// than the minimum height value.
TEST_F(ScrollViewTest, ClipHeightToShortContentHeight) {
  scroll_view_->ClipHeightTo(kMinHeight, kMaxHeight);

  const int kShortContentHeight = 10;
  View* contents =
      scroll_view_->SetContents(std::make_unique<views::StaticSizedView>(
          gfx::Size(kWidth, kShortContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroll_view_->GetPreferredSize({}));

  scroll_view_->SizeToPreferredSize();
  views::test::RunScheduledLayout(scroll_view_.get());

  // Layered scrolling requires the contents to fill the viewport.
  if (contents->layer()) {
    EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroll_view_->contents()->size());
  } else {
    EXPECT_EQ(gfx::Size(kWidth, kShortContentHeight),
              scroll_view_->contents()->size());
  }
  EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroll_view_->size());
}

// Verifies ClipHeightTo() uses the maximum height when the content is longer
// thamn the maximum height value.
TEST_F(ScrollViewTest, ClipHeightToTallContentHeight) {
  scroll_view_->ClipHeightTo(kMinHeight, kMaxHeight);

  const int kTallContentHeight = 1000;
  scroll_view_->SetContents(std::make_unique<views::StaticSizedView>(
      gfx::Size(kWidth, kTallContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view_->GetPreferredSize({}));

  scroll_view_->SizeToPreferredSize();
  views::test::RunScheduledLayout(scroll_view_.get());

  // The width may be less than kWidth if the scroll bar takes up some width.
  EXPECT_GE(kWidth, scroll_view_->contents()->width());
  EXPECT_EQ(kTallContentHeight, scroll_view_->contents()->height());
  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view_->size());
}

// Verifies that when ClipHeightTo() produces a scrollbar, it reduces the width
// of the inner content of the ScrollView.
TEST_F(ScrollViewTest, ClipHeightToScrollbarUsesWidth) {
  scroll_view_->ClipHeightTo(kMinHeight, kMaxHeight);

  // Create a view that will be much taller than it is wide.
  scroll_view_->SetContents(
      std::make_unique<views::ProportionallySizedView>(1000));

  // Without any width, it will default to 0,0 but be overridden by min height.
  scroll_view_->SizeToPreferredSize();
  EXPECT_EQ(gfx::Size(0, kMinHeight), scroll_view_->GetPreferredSize({}));

  gfx::Size new_size(kWidth, scroll_view_->GetHeightForWidth(kWidth));
  scroll_view_->SetSize(new_size);
  views::test::RunScheduledLayout(scroll_view_.get());

  int expected_width = kWidth - scroll_view_->GetScrollBarLayoutWidth();
  EXPECT_EQ(scroll_view_->contents()->size().width(), expected_width);
  EXPECT_EQ(scroll_view_->contents()->size().height(), 1000 * expected_width);
  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroll_view_->size());
}

// Verifies ClipHeightTo() updates the ScrollView's preferred size.
TEST_F(ScrollViewTest, ClipHeightToUpdatesPreferredSize) {
  auto contents_view = std::make_unique<View>();
  contents_view->SetPreferredSize(gfx::Size(100, 100));
  scroll_view_->SetContents(std::move(contents_view));
  EXPECT_FALSE(scroll_view_->is_bounded());

  constexpr int kMinHeight1 = 20;
  constexpr int kMaxHeight1 = 80;
  scroll_view_->ClipHeightTo(kMinHeight1, kMaxHeight1);
  EXPECT_TRUE(scroll_view_->is_bounded());
  EXPECT_EQ(scroll_view_->GetPreferredSize({}).height(), kMaxHeight1);

  constexpr int kMinHeight2 = 200;
  constexpr int kMaxHeight2 = 300;
  scroll_view_->ClipHeightTo(kMinHeight2, kMaxHeight2);
  EXPECT_EQ(scroll_view_->GetPreferredSize({}).height(), kMinHeight2);
}

TEST_F(ScrollViewTest, CornerViewVisibility) {
  View* contents = InstallContents();
  View* corner_view = ScrollViewTestApi(scroll_view_.get()).corner_view();

  contents->SetBounds(0, 0, 200, 200);
  InvalidateAndRunScheduledLayoutOnScrollView();

  // Corner view should not exist if using overlay scrollbars.
  if (scroll_view_->vertical_scroll_bar()->OverlapsContent()) {
    EXPECT_FALSE(corner_view->parent());
    return;
  }

  // Corner view should be visible when both scrollbars are visible.
  EXPECT_EQ(scroll_view_.get(), corner_view->parent());
  EXPECT_TRUE(corner_view->GetVisible());

  // Corner view should be aligned to the scrollbars.
  EXPECT_EQ(scroll_view_->vertical_scroll_bar()->x(), corner_view->x());
  EXPECT_EQ(scroll_view_->horizontal_scroll_bar()->y(), corner_view->y());
  EXPECT_EQ(scroll_view_->GetScrollBarLayoutWidth(), corner_view->width());
  EXPECT_EQ(scroll_view_->GetScrollBarLayoutHeight(), corner_view->height());

  // Corner view should be removed when only the vertical scrollbar is visible.
  contents->SetBounds(0, 0, 50, 200);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_FALSE(corner_view->parent());

  // ... or when only the horizontal scrollbar is visible.
  contents->SetBounds(0, 0, 200, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_FALSE(corner_view->parent());

  // ... or when no scrollbar is visible.
  contents->SetBounds(0, 0, 50, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_FALSE(corner_view->parent());

  // Corner view should reappear when both scrollbars reappear.
  contents->SetBounds(0, 0, 200, 200);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(scroll_view_.get(), corner_view->parent());
  EXPECT_TRUE(corner_view->GetVisible());
}

// This test needs a widget so that color changes will be reflected.
TEST_F(WidgetScrollViewTest, ChildWithLayerTest) {
  auto contents_ptr = std::make_unique<View>();
  auto* contents = contents_ptr.get();
  ScrollView* scroll_view = AddScrollViewWithContents(std::move(contents_ptr));
  ScrollViewTestApi test_api(scroll_view);

  if (test_api.contents_viewport()->layer())
    return;

  View* child = contents->AddChildView(std::make_unique<View>());
  child->SetPaintToLayer(ui::LAYER_TEXTURED);

  ASSERT_TRUE(test_api.contents_viewport()->layer());
  // The default ScrollView color is opaque, so that fills bounds opaquely
  // should be true.
  EXPECT_TRUE(test_api.contents_viewport()->layer()->fills_bounds_opaquely());

  // Setting a std::nullopt color should make fills opaquely false.
  scroll_view->SetBackgroundColor(std::nullopt);
  EXPECT_FALSE(test_api.contents_viewport()->layer()->fills_bounds_opaquely());

  child->DestroyLayer();
  EXPECT_FALSE(test_api.contents_viewport()->layer());

  child->AddChildView(std::make_unique<View>());
  EXPECT_FALSE(test_api.contents_viewport()->layer());
  child->SetPaintToLayer(ui::LAYER_TEXTURED);
  EXPECT_TRUE(test_api.contents_viewport()->layer());
}

// Validates that if a child of a ScrollView adds a layer, then a layer
// is not added to the ScrollView's viewport.
TEST_F(ScrollViewTest, DontCreateLayerOnViewportIfLayerOnScrollViewCreated) {
  View* contents = InstallContents();
  ScrollViewTestApi test_api(scroll_view_.get());

  if (test_api.contents_viewport()->layer())
    return;

  scroll_view_->SetPaintToLayer();

  View* child = contents->AddChildView(std::make_unique<View>());
  child->SetPaintToLayer(ui::LAYER_TEXTURED);

  EXPECT_FALSE(test_api.contents_viewport()->layer());
}

// Validates if the contents_viewport uses correct layer type when adding views
// with different types of layers.
TEST_F(ScrollViewTest, ContentsViewportLayerUsed_ScrollWithLayersDisabled) {
  // Disabling scroll_with_layers feature explicitly.
  ScrollView scroll_view(ScrollView::ScrollWithLayers::kDisabled);
  ScrollViewTestApi test_api(&scroll_view);

  View* contents = scroll_view.SetContents(std::make_unique<View>());

  ASSERT_FALSE(test_api.contents_viewport()->layer());

  View* child = contents->AddChildView(std::make_unique<View>());
  child->SetPaintToLayer();

  // When contents does not have a layer, contents_viewport is TEXTURED layer.
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_TEXTURED);
  contents->SetPaintToLayer();
  // When contents is a TEXTURED layer.
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_NOT_DRAWN);
  contents->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  // When contents is a NOT_DRAWN layer.
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_TEXTURED);
}

// Validates the layer of contents_viewport_, when contents_ does not have a
// layer.
TEST_F(
    ScrollViewTest,
    ContentsViewportLayerWhenContentsDoesNotHaveLayer_ScrollWithLayersDisabled) {
  // Disabling scroll_with_layers feature explicitly.
  ScrollView scroll_view(ScrollView::ScrollWithLayers::kDisabled);
  ScrollViewTestApi test_api(&scroll_view);

  auto contents = std::make_unique<View>();
  View* child = contents->AddChildView(std::make_unique<View>());
  contents->AddChildView(std::make_unique<View>());

  scroll_view.SetContents(std::move(contents));
  // No layer needed for contents_viewport since no descendant view has a layer.
  EXPECT_FALSE(test_api.contents_viewport()->layer());
  child->SetPaintToLayer();
  // TEXTURED layer needed for contents_viewport since a descendant view has a
  // layer.
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_TEXTURED);
}

// Validates if scroll_with_layers is enabled, we disallow to change the layer
// of contents_  once the contents of ScrollView are set.
TEST_F(
    ScrollViewTest,
    ContentsLayerCannotBeChangedAfterContentsAreSet_ScrollWithLayersEnabled) {
  ScrollView scroll_view(ScrollView::ScrollWithLayers::kEnabled);
  ScrollViewTestApi test_api(&scroll_view);

  View* contents = scroll_view.SetContents(std::make_unique<View>());
  EXPECT_DCHECK_DEATH(contents->SetPaintToLayer(ui::LAYER_NOT_DRAWN));
}

// Validates if scroll_with_layers is disabled, we can change the layer of
// contents_ once the contents of ScrollView are set.
TEST_F(ScrollViewTest,
       ContentsLayerCanBeChangedAfterContentsAreSet_ScrollWithLayersDisabled) {
  ScrollView scroll_view(ScrollView::ScrollWithLayers::kDisabled);
  ScrollViewTestApi test_api(&scroll_view);

  View* contents = scroll_view.SetContents(std::make_unique<View>());
  ASSERT_NO_FATAL_FAILURE(contents->SetPaintToLayer());
}

// Validates if the content of contents_viewport is changed, a correct layer is
// used for contents_viewport.
TEST_F(
    ScrollViewTest,
    ContentsViewportLayerUsedWhenScrollViewContentsAreChanged_ScrollWithLayersDisabled) {
  // Disabling scroll_with_layers feature explicitly.
  ScrollView scroll_view(ScrollView::ScrollWithLayers::kDisabled);
  ScrollViewTestApi test_api(&scroll_view);

  auto contents = std::make_unique<View>();
  contents->AddChildView(std::make_unique<View>());
  scroll_view.SetContents(std::move(contents));

  // Replacing the old contents of scroll view.
  auto a_view = std::make_unique<View>();
  a_view->AddChildView(std::make_unique<View>());
  View* child = a_view->AddChildView(std::make_unique<View>());
  child->SetPaintToLayer();

  scroll_view.SetContents(std::move(a_view));
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_TEXTURED);
}

// Validates correct behavior of layers used for contents_viewport used when
// scroll with layers is enabled.
TEST_F(ScrollViewTest, ContentsViewportLayerUsed_ScrollWithLayersEnabled) {
  ScrollView scroll_view(ScrollView::ScrollWithLayers::kEnabled);
  ScrollViewTestApi test_api(&scroll_view);

  // scroll_with_layer feature ensures that contents_viewport always have a
  // layer.
  ASSERT_TRUE(test_api.contents_viewport()->layer());
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_NOT_DRAWN);
  // scroll_with_layer feature enables a layer on content before adding to
  // contents_viewport_.
  View* contents = scroll_view.SetContents(std::make_unique<View>());
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_NOT_DRAWN);

  View* child = contents->AddChildView(std::make_unique<View>());
  child->SetPaintToLayer();

  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_NOT_DRAWN);
}

// Validates if correct layers are used for contents_viewport used when
// ScrollView enables a NOT_DRAWN layer on contents when scroll with layers in
// enabled.
TEST_F(
    ScrollViewTest,
    ContentsViewportLayerUsedWhenNotDrawnUsedForContents_ScrollWithLayersEnabled) {
  ScrollView scroll_view(ScrollView::ScrollWithLayers::kEnabled);
  ScrollViewTestApi test_api(&scroll_view);

  // scroll_with_layer feature ensures that contents_viewport always have a
  // layer.
  ASSERT_TRUE(test_api.contents_viewport()->layer());
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_NOT_DRAWN);

  // changing the layer type that the scrollview enables on contents.
  scroll_view.SetContentsLayerType(ui::LAYER_NOT_DRAWN);

  View* contents = scroll_view.SetContents(std::make_unique<View>());
  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_TEXTURED);

  View* child = contents->AddChildView(std::make_unique<View>());
  child->SetPaintToLayer();

  EXPECT_EQ(test_api.contents_viewport()->layer()->type(), ui::LAYER_TEXTURED);
}

TEST_F(ScrollViewTest,
       ContentsViewportLayerHasRoundedCorners_ScrollWithLayersEnabled) {
  ScrollView scroll_view(ScrollView::ScrollWithLayers::kEnabled);
  ScrollViewTestApi test_api(&scroll_view);
  ASSERT_TRUE(test_api.contents_viewport()->layer());

  const gfx::RoundedCornersF corner_radii = gfx::RoundedCornersF{16};
  scroll_view.SetViewportRoundedCornerRadius(corner_radii);

  EXPECT_EQ(test_api.contents_viewport()->layer()->rounded_corner_radii(),
            corner_radii);
}

#if BUILDFLAG(IS_MAC)
// Tests the overlay scrollbars on Mac. Ensure that they show up properly and
// do not overlap each other.
TEST_F(ScrollViewTest, CocoaOverlayScrollBars) {
  SetOverlayScrollersEnabled(true);
  View* contents = InstallContents();

  // Size the contents such that vertical scrollbar is needed.
  // Since it is overlaid, the ViewPort size should match the ScrollView.
  contents->SetBounds(0, 0, 50, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view_->GetScrollBarLayoutWidth());
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, false);

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view_->GetScrollBarLayoutHeight());
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, false);
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, true);

  // Both horizontal and vertical scrollbars.
  contents->SetBounds(0, 0, 300, 400);
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_EQ(0, scroll_view_->GetScrollBarLayoutWidth());
  EXPECT_EQ(0, scroll_view_->GetScrollBarLayoutHeight());
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, true);

  // Make sure the horizontal and vertical scrollbars don't overlap each other.
  gfx::Rect vert_bounds = scroll_view_->vertical_scroll_bar()->bounds();
  gfx::Rect horiz_bounds = scroll_view_->horizontal_scroll_bar()->bounds();
  EXPECT_EQ(vert_bounds.x(), horiz_bounds.right());
  EXPECT_EQ(horiz_bounds.y(), vert_bounds.bottom());

  // Switch to the non-overlay style and check that the ViewPort is now sized
  // to be smaller, and ScrollbarWidth and ScrollbarHeight are non-zero.
  SetOverlayScrollersEnabled(false);
  EXPECT_TRUE(ViewTestApi(scroll_view_.get()).needs_layout());
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(100 - VerticalScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100 - HorizontalScrollBarHeight(), contents->parent()->height());
  EXPECT_NE(0, VerticalScrollBarWidth());
  EXPECT_NE(0, HorizontalScrollBarHeight());
}

// Test that overlay scroll bars will only process events when visible.
TEST_F(WidgetScrollViewTest,
       OverlayScrollBarsCannotProcessEventsWhenTransparent) {
  // Allow expectations to distinguish between fade outs and immediate changes.
  ui::ScopedAnimationDurationScaleMode really_animate(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  SetUseOverlayScrollers();

  ScrollView* scroll_view = AddScrollViewWithContentSize(
      gfx::Size(kDefaultWidth * 5, kDefaultHeight * 5));
  ScrollViewTestApi test_api(scroll_view);
  ScrollBar* scroll_bar = test_api.GetScrollBar(HORIZONTAL);

  // Verify scroll bar is unable to process events.
  EXPECT_FALSE(scroll_bar->GetCanProcessEventsWithinSubtree());

  ui::test::EventGenerator generator(
      GetContext(), scroll_view->GetWidget()->GetNativeWindow());

  generator.GenerateTrackpadRest();

  // Since the scroll bar will become visible, it should now be able to process
  // events.
  EXPECT_TRUE(scroll_bar->GetCanProcessEventsWithinSubtree());
}

// Test overlay scrollbar behavior when just resting fingers on the trackpad.
TEST_F(WidgetScrollViewTest, ScrollersOnRest) {
  // Allow expectations to distinguish between fade outs and immediate changes.
  ui::ScopedAnimationDurationScaleMode really_animate(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const float kMaxOpacity = 0.8f;  // Constant from cocoa_scroll_bar.mm.

  SetUseOverlayScrollers();

  // Set up with both scrollers.
  ScrollView* scroll_view = AddScrollViewWithContentSize(
      gfx::Size(kDefaultWidth * 5, kDefaultHeight * 5));
  ScrollViewTestApi test_api(scroll_view);
  const auto bar = std::to_array<ScrollBar*>(
      {test_api.GetScrollBar(HORIZONTAL), test_api.GetScrollBar(VERTICAL)});
  const auto hide_timer = std::to_array<base::RetainingOneShotTimer*>(
      {test_api.GetScrollBarHideTimer(HORIZONTAL),
       test_api.GetScrollBarHideTimer(VERTICAL)});

  EXPECT_EQ(0, bar[HORIZONTAL]->layer()->opacity());
  EXPECT_EQ(0, bar[VERTICAL]->layer()->opacity());

  ui::test::EventGenerator generator(
      GetContext(), scroll_view->GetWidget()->GetNativeWindow());

  generator.GenerateTrackpadRest();
  // Scrollers should be max opacity without an animation.
  EXPECT_EQ(kMaxOpacity, bar[HORIZONTAL]->layer()->opacity());
  EXPECT_EQ(kMaxOpacity, bar[VERTICAL]->layer()->opacity());
  EXPECT_FALSE(hide_timer[HORIZONTAL]->IsRunning());
  EXPECT_FALSE(hide_timer[VERTICAL]->IsRunning());

  generator.CancelTrackpadRest();
  // Scrollers should start fading out, but only after a delay.
  for (ScrollBarOrientation orientation : {HORIZONTAL, VERTICAL}) {
    EXPECT_EQ(kMaxOpacity, bar[orientation]->layer()->GetTargetOpacity());
    EXPECT_TRUE(hide_timer[orientation]->IsRunning());
    // Trigger the timer. Should then be fading out.
    hide_timer[orientation]->user_task().Run();
    hide_timer[orientation]->Stop();
    EXPECT_EQ(0, bar[orientation]->layer()->GetTargetOpacity());
  }

  // Rest again.
  generator.GenerateTrackpadRest();
  EXPECT_EQ(kMaxOpacity, bar[HORIZONTAL]->layer()->GetTargetOpacity());
  EXPECT_EQ(kMaxOpacity, bar[VERTICAL]->layer()->GetTargetOpacity());

  // Scroll vertically.
  const float y_offset = 3;
  const int kSteps = 1;
  const int kNnumFingers = 2;
  generator.ScrollSequence(generator.current_screen_location(),
                           base::TimeDelta(), 0, y_offset, kSteps,
                           kNnumFingers);

  // Horizontal scroller should start fading out immediately.
  EXPECT_EQ(kMaxOpacity, bar[HORIZONTAL]->layer()->opacity());
  EXPECT_EQ(0, bar[HORIZONTAL]->layer()->GetTargetOpacity());
  EXPECT_FALSE(hide_timer[HORIZONTAL]->IsRunning());

  // Vertical should remain visible, but ready to fade out after a delay.
  EXPECT_EQ(kMaxOpacity, bar[VERTICAL]->layer()->opacity());
  EXPECT_EQ(kMaxOpacity, bar[VERTICAL]->layer()->GetTargetOpacity());
  EXPECT_TRUE(hide_timer[VERTICAL]->IsRunning());

  // Scrolling should have occurred.
  EXPECT_EQ(gfx::PointF(0, y_offset), test_api.CurrentOffset());

  // Then, scrolling horizontally should show the horizontal scroller. The
  // vertical scroller should still be visible, running its hide timer.
  const float x_offset = 5;
  generator.ScrollSequence(generator.current_screen_location(),
                           base::TimeDelta(), x_offset, 0, kSteps,
                           kNnumFingers);
  for (ScrollBarOrientation orientation : {HORIZONTAL, VERTICAL}) {
    EXPECT_EQ(kMaxOpacity, bar[orientation]->layer()->opacity());
    EXPECT_EQ(kMaxOpacity, bar[orientation]->layer()->GetTargetOpacity());
    EXPECT_TRUE(hide_timer[orientation]->IsRunning());
  }

  // Now scrolling has occurred in both directions.
  EXPECT_EQ(gfx::PointF(x_offset, y_offset), test_api.CurrentOffset());
}

#endif  // BUILDFLAG(IS_MAC)

// Test that increasing the size of the viewport "below" scrolled content causes
// the content to scroll up so that it still fills the viewport.
TEST_F(ScrollViewTest, ConstrainScrollToBounds) {
  ScrollViewTestApi test_api(scroll_view_.get());

  View* contents = InstallContents();
  contents->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  InvalidateAndRunScheduledLayoutOnScrollView();

  EXPECT_EQ(gfx::PointF(), test_api.CurrentOffset());

  // Scroll as far as it goes and query location to discount scroll bars.
  contents->ScrollRectToVisible(gfx::Rect(300, 300, 1, 1));
  const gfx::PointF fully_scrolled = test_api.CurrentOffset();
  EXPECT_NE(gfx::PointF(), fully_scrolled);

  // Making the viewport 55 pixels taller should scroll up the same amount.
  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 155));
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(fully_scrolled.y() - 55, test_api.CurrentOffset().y());
  EXPECT_EQ(fully_scrolled.x(), test_api.CurrentOffset().x());

  // And 77 pixels wider should scroll left. Also make it short again: the y-
  // offset from the last change should remain.
  scroll_view_->SetBoundsRect(gfx::Rect(0, 0, 177, 100));
  InvalidateAndRunScheduledLayoutOnScrollView();
  EXPECT_EQ(fully_scrolled.y() - 55, test_api.CurrentOffset().y());
  EXPECT_EQ(fully_scrolled.x() - 77, test_api.CurrentOffset().x());
}

// Calling Layout on ScrollView should not reset the scroll location.
TEST_F(ScrollViewTest, ContentScrollNotResetOnLayout) {
  ScrollViewTestApi test_api(scroll_view_.get());

  auto* contents = scroll_view_->SetContents(std::make_unique<CustomView>());

  contents->SetPreferredSize(gfx::Size(300, 300));
  scroll_view_->ClipHeightTo(0, 150);
  scroll_view_->SizeToPreferredSize();
  // ScrollView preferred width matches that of |contents|, with the height
  // capped at the value we clipped to.
  EXPECT_EQ(gfx::Size(300, 150), scroll_view_->size());

  // Scroll down.
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), 25);
  EXPECT_EQ(25, test_api.CurrentOffset().y());
  // Call Layout; no change to scroll position.
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(25, test_api.CurrentOffset().y());
  // Change contents of |contents|, call Layout; still no change to scroll
  // position.
  contents->SetPreferredSize(gfx::Size(300, 500));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(25, test_api.CurrentOffset().y());

  // Change |contents| to be shorter than the ScrollView's clipped height.
  // This /will/ change the scroll location due to ConstrainScrollToBounds.
  contents->SetPreferredSize(gfx::Size(300, 50));
  views::test::RunScheduledLayout(scroll_view_.get());
  EXPECT_EQ(0, test_api.CurrentOffset().y());
}

TEST_F(ScrollViewTest, ArrowKeyScrolling) {
  // Set up with vertical scrollbar.
  auto contents = std::make_unique<FixedView>();
  contents->SetPreferredSize(gfx::Size(kWidth, kMaxHeight * 5));
  scroll_view_->SetContents(std::move(contents));
  scroll_view_->ClipHeightTo(0, kMaxHeight);
  scroll_view_->SetSize(gfx::Size(kWidth, kMaxHeight));
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);

  // The vertical position starts at 0.
  ScrollViewTestApi test_api(scroll_view_.get());
  EXPECT_EQ(0, test_api.IntegralViewOffset().y());

  // Pressing the down arrow key scrolls down. The amount isn't important.
  ui::KeyEvent down_arrow(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                          ui::EF_NONE);
  EXPECT_TRUE(scroll_view_->OnKeyPressed(down_arrow));
  EXPECT_GT(0, test_api.IntegralViewOffset().y());

  // Pressing the up arrow key scrolls back to the origin.
  ui::KeyEvent up_arrow(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE);
  EXPECT_TRUE(scroll_view_->OnKeyPressed(up_arrow));
  EXPECT_EQ(0, test_api.IntegralViewOffset().y());
}

TEST_F(ScrollViewTest, ArrowKeyScrollingDisabled) {
  // Set up with vertical scrollbar.
  auto contents = std::make_unique<FixedView>();
  contents->SetPreferredSize(gfx::Size(kWidth, kMaxHeight * 5));
  scroll_view_->SetContents(std::move(contents));
  scroll_view_->ClipHeightTo(0, kMaxHeight);
  scroll_view_->SetSize(gfx::Size(kWidth, kMaxHeight));
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);

  // Disable keyboard scrolling.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // The vertical position starts at 0.
  ScrollViewTestApi test_api(scroll_view_.get());
  EXPECT_EQ(0, test_api.IntegralViewOffset().y());

  // Pressing the down arrow key does not consume the event, nor scroll.
  ui::KeyEvent down(ui::EventType::kKeyPressed, ui::VKEY_DOWN, ui::EF_NONE);
  EXPECT_FALSE(scroll_view_->OnKeyPressed(down));
  EXPECT_EQ(0, test_api.IntegralViewOffset().y());
}

// Test that overflow indicators turn on appropriately.
TEST_F(ScrollViewTest, VerticalOverflowIndicators) {
  ScrollViewTestApi test_api(scroll_view_.get());

  // Set up with vertical scrollbar.
  auto contents = std::make_unique<FixedView>();
  contents->SetPreferredSize(gfx::Size(kWidth, kMaxHeight * 5));
  scroll_view_->SetContents(std::move(contents));
  scroll_view_->ClipHeightTo(0, kMaxHeight);

  // Make sure the size is set such that no horizontal scrollbar gets shown.
  scroll_view_->SetSize(gfx::Size(
      kWidth + test_api.GetScrollBar(VERTICAL)->GetThickness(), kMaxHeight));

  // Make sure the initial origin is 0,0
  EXPECT_EQ(gfx::PointF(0, 0), test_api.CurrentOffset());

  // The vertical scroll bar should be visible and the horizontal scroll bar
  // should not.
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, false);

  // The overflow indicator on the bottom should be visible.
  EXPECT_TRUE(test_api.more_content_bottom()->GetVisible());

  // The overflow indicator on the top should not be visible.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());

  // No other overflow indicators should be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // Now scroll the view to someplace in the middle of the scrollable region.
  int offset = kMaxHeight * 2;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), offset);
  EXPECT_EQ(gfx::PointF(0, offset), test_api.CurrentOffset());

  // At this point, both overflow indicators on the top and bottom should be
  // visible.
  EXPECT_TRUE(test_api.more_content_top()->GetVisible());
  EXPECT_TRUE(test_api.more_content_bottom()->GetVisible());

  // The left and right overflow indicators should still not be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // Finally scroll the view to end of the scrollable region.
  offset = kMaxHeight * 4;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), offset);
  EXPECT_EQ(gfx::PointF(0, offset), test_api.CurrentOffset());

  // The overflow indicator on the bottom should not be visible.
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // The overflow indicator on the top should be visible.
  EXPECT_TRUE(test_api.more_content_top()->GetVisible());

  // As above, no other overflow indicators should be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());
}

TEST_F(ScrollViewTest, HorizontalOverflowIndicators) {
  const int kHeight = 100;

  ScrollViewTestApi test_api(scroll_view_.get());

  // Set up with horizontal scrollbar.
  auto* contents = scroll_view_->SetContents(std::make_unique<FixedView>());
  contents->SetPreferredSize(gfx::Size(kWidth * 5, kHeight));

  // Make sure the size is set such that no vertical scrollbar gets shown.
  scroll_view_->SetSize(gfx::Size(
      kWidth, kHeight + test_api.GetScrollBar(HORIZONTAL)->GetThickness()));

  contents->SetBounds(0, 0, kWidth * 5, kHeight);

  // Make sure the initial origin is 0,0
  EXPECT_EQ(gfx::PointF(0, 0), test_api.CurrentOffset());

  // The horizontal scroll bar should be visible and the vertical scroll bar
  // should not.
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, true);
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, false);

  // The overflow indicator on the right should be visible.
  EXPECT_TRUE(test_api.more_content_right()->GetVisible());

  // The overflow indicator on the left should not be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());

  // No other overflow indicators should be visible.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // Now scroll the view to someplace in the middle of the scrollable region.
  int offset = kWidth * 2;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(HORIZONTAL), offset);
  EXPECT_EQ(gfx::PointF(offset, 0), test_api.CurrentOffset());

  // At this point, both overflow indicators on the left and right should be
  // visible.
  EXPECT_TRUE(test_api.more_content_left()->GetVisible());
  EXPECT_TRUE(test_api.more_content_right()->GetVisible());

  // The top and bottom overflow indicators should still not be visible.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // Finally scroll the view to end of the scrollable region.
  offset = kWidth * 4;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(HORIZONTAL), offset);
  EXPECT_EQ(gfx::PointF(offset, 0), test_api.CurrentOffset());

  // The overflow indicator on the right should not be visible.
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // The overflow indicator on the left should be visible.
  EXPECT_TRUE(test_api.more_content_left()->GetVisible());

  // As above, no other overflow indicators should be visible.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());
}

TEST_F(ScrollViewTest, HorizontalVerticalOverflowIndicators) {
  const int kHeight = 100;

  ScrollViewTestApi test_api(scroll_view_.get());

  // Set up with both horizontal and vertical scrollbars.
  auto contents = std::make_unique<FixedView>();
  contents->SetPreferredSize(gfx::Size(kWidth * 5, kHeight * 5));
  scroll_view_->SetContents(std::move(contents));

  // Make sure the size is set such that both scrollbars are shown.
  scroll_view_->SetSize(gfx::Size(kWidth, kHeight));

  // Make sure the initial origin is 0,0
  EXPECT_EQ(gfx::PointF(0, 0), test_api.CurrentOffset());

  // The horizontal and vertical scroll bars should be visible.
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, true);
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);

  // The overflow indicators on the right and bottom should not be visible since
  // they are against the scrollbars.
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // The overflow indicators on the left and top should not be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());

  // Now scroll the view to someplace in the middle of the horizontal scrollable
  // region.
  int offset_x = kWidth * 2;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(HORIZONTAL), offset_x);
  EXPECT_EQ(gfx::PointF(offset_x, 0), test_api.CurrentOffset());

  // Since there is a vertical scrollbar only the overflow indicator on the left
  // should be visible and the one on the right should still not be visible.
  EXPECT_TRUE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // The top and bottom overflow indicators should still not be visible.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // Next, scroll the view to end of the scrollable region.
  offset_x = kWidth * 4;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(HORIZONTAL), offset_x);
  EXPECT_EQ(gfx::PointF(offset_x, 0), test_api.CurrentOffset());

  // The overflow indicator on the right should still not be visible.
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // The overflow indicator on the left should be visible.
  EXPECT_TRUE(test_api.more_content_left()->GetVisible());

  // As above, the other overflow indicators should not be visible because the
  // view hasn't scrolled vertically and the bottom indicator is against the
  // horizontal scrollbar.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // Return the view back to the horizontal origin.
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(HORIZONTAL), 0);
  EXPECT_EQ(gfx::PointF(0, 0), test_api.CurrentOffset());

  // The overflow indicators on the right and bottom should not be visible since
  // they are against the scrollbars.
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // The overflow indicators on the left and top should not be visible since the
  // is at the origin.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());

  // Now scroll the view to somplace in the middle of the vertical scrollable
  // region.
  int offset_y = kHeight * 2;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), offset_y);
  EXPECT_EQ(gfx::PointF(0, offset_y), test_api.CurrentOffset());

  // Similar to the above, since there is a horizontal scrollbar only the
  // overflow indicator on the top should be visible and the one on the bottom
  // should still not be visible.
  EXPECT_TRUE(test_api.more_content_top()->GetVisible());
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // The left and right overflow indicators should still not be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // Finally, for the vertical test scroll the region all the way to the end.
  offset_y = kHeight * 4;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), offset_y);
  EXPECT_EQ(gfx::PointF(0, offset_y), test_api.CurrentOffset());

  // The overflow indicator on the bottom should still not be visible.
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // The overflow indicator on the top should still be visible.
  EXPECT_TRUE(test_api.more_content_top()->GetVisible());

  // As above, the other overflow indicators should not be visible because the
  // view hasn't scrolled horizontally and the right indicator is against the
  // vertical scrollbar.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // Back to the horizontal. Scroll all the way to the end in the horizontal
  // direction.
  offset_x = kWidth * 4;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(HORIZONTAL), offset_x);
  EXPECT_EQ(gfx::PointF(offset_x, offset_y), test_api.CurrentOffset());

  // The overflow indicator on the bottom and right should still not be visible.
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // The overflow indicators on the top and left should now be visible.
  EXPECT_TRUE(test_api.more_content_top()->GetVisible());
  EXPECT_TRUE(test_api.more_content_left()->GetVisible());
}

TEST_F(ScrollViewTest, VerticalWithHeaderOverflowIndicators) {
  ScrollViewTestApi test_api(scroll_view_.get());

  // Set up with vertical scrollbar and a header.
  auto contents = std::make_unique<FixedView>();
  auto header = std::make_unique<CustomView>();
  contents->SetPreferredSize(gfx::Size(kWidth, kMaxHeight * 5));
  header->SetPreferredSize(gfx::Size(10, 20));
  scroll_view_->SetContents(std::move(contents));
  auto* header_ptr = scroll_view_->SetHeader(std::move(header));
  scroll_view_->ClipHeightTo(0, kMaxHeight + header_ptr->height());

  // Make sure the size is set such that no horizontal scrollbar gets shown.
  scroll_view_->SetSize(
      gfx::Size(kWidth + test_api.GetScrollBar(VERTICAL)->GetThickness(),
                kMaxHeight + header_ptr->height()));

  // Make sure the initial origin is 0,0
  EXPECT_EQ(gfx::PointF(0, 0), test_api.CurrentOffset());

  // The vertical scroll bar should be visible and the horizontal scroll bar
  // should not.
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, true);
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, false);

  // The overflow indicator on the bottom should be visible.
  EXPECT_TRUE(test_api.more_content_bottom()->GetVisible());

  // The overflow indicator on the top should not be visible.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());

  // No other overflow indicators should be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // Now scroll the view to someplace in the middle of the scrollable region.
  int offset = kMaxHeight * 2;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), offset);
  EXPECT_EQ(gfx::PointF(0, offset), test_api.CurrentOffset());

  // At this point, only the overflow indicator on the bottom should be visible
  // because the top indicator never comes on because of the presence of the
  // header.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());
  EXPECT_TRUE(test_api.more_content_bottom()->GetVisible());

  // The left and right overflow indicators should still not be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());

  // Finally scroll the view to end of the scrollable region.
  offset = test_api.GetScrollBar(VERTICAL)->GetMaxPosition();
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), offset);
  EXPECT_EQ(gfx::PointF(0, offset), test_api.CurrentOffset());

  // The overflow indicator on the bottom should not be visible now.
  EXPECT_FALSE(test_api.more_content_bottom()->GetVisible());

  // The overflow indicator on the top should still not be visible.
  EXPECT_FALSE(test_api.more_content_top()->GetVisible());

  // As above, no other overflow indicators should be visible.
  EXPECT_FALSE(test_api.more_content_left()->GetVisible());
  EXPECT_FALSE(test_api.more_content_right()->GetVisible());
}

TEST_F(ScrollViewTest, CustomOverflowIndicator) {
  const int kHeight = 100;

  ScrollViewTestApi test_api(scroll_view_.get());

  // Set up with both horizontal and vertical scrolling.
  auto contents = std::make_unique<FixedView>();
  contents->SetPreferredSize(gfx::Size(kWidth * 5, kHeight * 5));
  scroll_view_->SetContents(std::move(contents));

  // Hide both scrollbars so they don't interfere with indicator visibility.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  // Make sure the size is set so the ScrollView is smaller than its contents
  // in both directions.
  scroll_view_->SetSize(gfx::Size(kWidth, kHeight));

  // The horizontal and vertical scroll bars should not be visible.
  CheckScrollbarVisibility(scroll_view_.get(), HORIZONTAL, false);
  CheckScrollbarVisibility(scroll_view_.get(), VERTICAL, false);

  // Make sure the initial origin is 0,0
  EXPECT_EQ(gfx::PointF(0, 0), test_api.CurrentOffset());

  // Now scroll the view to someplace in the middle of the scrollable region.
  int offset_x = kWidth * 2;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(HORIZONTAL), offset_x);
  int offset_y = kHeight * 2;
  scroll_view_->ScrollToPosition(test_api.GetScrollBar(VERTICAL), offset_y);
  EXPECT_EQ(gfx::PointF(offset_x, offset_y), test_api.CurrentOffset());

  // All overflow indicators should be visible.
  ASSERT_TRUE(test_api.more_content_right()->GetVisible());
  ASSERT_TRUE(test_api.more_content_bottom()->GetVisible());
  ASSERT_TRUE(test_api.more_content_left()->GetVisible());
  ASSERT_TRUE(test_api.more_content_top()->GetVisible());

  // This should be similar to the default separator.
  View* left_indicator = scroll_view_->SetCustomOverflowIndicator(
      OverflowIndicatorAlignment::kLeft, std::make_unique<View>(), 1, true);
  EXPECT_EQ(gfx::Rect(0, 0, 1, 100), left_indicator->bounds());
  if (left_indicator->layer())
    EXPECT_TRUE(left_indicator->layer()->fills_bounds_opaquely());

  // A larger, but still reasonable, indicator that is not opaque.
  View* top_indicator = scroll_view_->SetCustomOverflowIndicator(
      OverflowIndicatorAlignment::kTop, std::make_unique<View>(), 20, false);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 20), top_indicator->bounds());
  if (top_indicator->layer())
    EXPECT_FALSE(top_indicator->layer()->fills_bounds_opaquely());

  // Negative thickness doesn't make sense. It should be treated like zero.
  View* right_indicator = scroll_view_->SetCustomOverflowIndicator(
      OverflowIndicatorAlignment::kRight, std::make_unique<View>(), -1, true);
  EXPECT_EQ(gfx::Rect(100, 0, 0, 100), right_indicator->bounds());

  // Thicker than the scrollview is strange, but works as you'd expect.
  View* bottom_indicator = scroll_view_->SetCustomOverflowIndicator(
      OverflowIndicatorAlignment::kBottom, std::make_unique<View>(), 1000,
      true);
  EXPECT_EQ(gfx::Rect(0, -900, 100, 1000), bottom_indicator->bounds());
}

// Ensure ScrollView::Layout succeeds if a disabled scrollbar's overlap style
// does not match the other scrollbar.
TEST_F(ScrollViewTest, IgnoreOverlapWithDisabledHorizontalScroll) {
  ScrollViewTestApi test_api(scroll_view_.get());

  constexpr int kThickness = 1;
  // Assume horizontal scroll bar is the default and is overlapping.
  scroll_view_->SetHorizontalScrollBar(
      std::make_unique<TestScrollBar>(ScrollBar::Orientation::kHorizontal,
                                      /*overlaps_content=*/true, kThickness));
  // Assume vertical scroll bar is custom and it we want it to not overlap.
  scroll_view_->SetVerticalScrollBar(
      std::make_unique<TestScrollBar>(ScrollBar::Orientation::kVertical,
                                      /*overlaps_content=*/false, kThickness));

  // Also, let's turn off horizontal scroll bar.
  scroll_view_->SetHorizontalScrollBarMode(
      ScrollView::ScrollBarMode::kDisabled);

  View* contents = InstallContents();
  contents->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  InvalidateAndRunScheduledLayoutOnScrollView();

  gfx::Size expected_size = scroll_view_->size();
  expected_size.Enlarge(-kThickness, 0);
  EXPECT_EQ(expected_size, test_api.contents_viewport()->size());
}

// Ensure ScrollView::Layout succeeds if a hidden but enabled scrollbar's
// overlap style does not match the other scrollbar.
TEST_F(ScrollViewTest, IgnoreOverlapWithHiddenHorizontalScroll) {
  ScrollViewTestApi test_api(scroll_view_.get());

  constexpr int kThickness = 1;
  // Assume horizontal scroll bar is the default and is overlapping.
  scroll_view_->SetHorizontalScrollBar(
      std::make_unique<TestScrollBar>(ScrollBar::Orientation::kHorizontal,
                                      /*overlaps_content=*/true, kThickness));
  // Assume vertical scroll bar is custom and it we want it to not overlap.
  scroll_view_->SetVerticalScrollBar(
      std::make_unique<TestScrollBar>(ScrollBar::Orientation::kVertical,
                                      /*overlaps_content=*/false, kThickness));

  // Also, let's turn off horizontal scroll bar.
  scroll_view_->SetHorizontalScrollBarMode(
      ScrollView::ScrollBarMode::kHiddenButEnabled);

  View* contents = InstallContents();
  contents->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  InvalidateAndRunScheduledLayoutOnScrollView();

  gfx::Size expected_size = scroll_view_->size();
  expected_size.Enlarge(-kThickness, 0);
  EXPECT_EQ(expected_size, test_api.contents_viewport()->size());
}

// Ensure ScrollView::Layout succeeds if a disabled scrollbar's overlap style
// does not match the other scrollbar.
TEST_F(ScrollViewTest, IgnoreOverlapWithDisabledVerticalScroll) {
  ScrollViewTestApi test_api(scroll_view_.get());

  constexpr int kThickness = 1;
  // Assume horizontal scroll bar is custom and it we want it to not overlap.
  scroll_view_->SetHorizontalScrollBar(
      std::make_unique<TestScrollBar>(ScrollBar::Orientation::kHorizontal,
                                      /*overlaps_content=*/false, kThickness));
  // Assume vertical scroll bar is the default and is overlapping.
  scroll_view_->SetVerticalScrollBar(
      std::make_unique<TestScrollBar>(ScrollBar::Orientation::kVertical,
                                      /*overlaps_content=*/true, kThickness));

  // Also, let's turn off horizontal scroll bar.
  scroll_view_->SetVerticalScrollBarMode(ScrollView::ScrollBarMode::kDisabled);

  View* contents = InstallContents();
  contents->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  InvalidateAndRunScheduledLayoutOnScrollView();

  gfx::Size expected_size = scroll_view_->size();
  expected_size.Enlarge(0, -kThickness);
  EXPECT_EQ(expected_size, test_api.contents_viewport()->size());
}

// Ensure ScrollView::Layout succeeds if a hidden but enabled scrollbar's
// overlap style does not match the other scrollbar.
TEST_F(ScrollViewTest, IgnoreOverlapWithHiddenVerticalScroll) {
  ScrollViewTestApi test_api(scroll_view_.get());

  constexpr int kThickness = 1;
  // Assume horizontal scroll bar is custom and it we want it to not overlap.
  scroll_view_->SetHorizontalScrollBar(
      std::make_unique<TestScrollBar>(ScrollBar::Orientation::kHorizontal,
                                      /*overlaps_content=*/false, kThickness));
  // Assume vertical scroll bar is the default and is overlapping.
  scroll_view_->SetVerticalScrollBar(
      std::make_unique<TestScrollBar>(ScrollBar::Orientation::kVertical,
                                      /*overlaps_content=*/true, kThickness));

  // Also, let's turn off horizontal scroll bar.
  scroll_view_->SetVerticalScrollBarMode(
      ScrollView::ScrollBarMode::kHiddenButEnabled);

  View* contents = InstallContents();
  contents->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  InvalidateAndRunScheduledLayoutOnScrollView();

  gfx::Size expected_size = scroll_view_->size();
  expected_size.Enlarge(0, -kThickness);
  EXPECT_EQ(expected_size, test_api.contents_viewport()->size());
}

TEST_F(ScrollViewTest, TestSettingContentsToNull) {
  View* contents = InstallContents();
  ViewTracker tracker(contents);
  ASSERT_TRUE(tracker.view());

  // Make sure the content is installed and working.
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());

  // This should be legal and not DCHECK.
  scroll_view_->SetContents(nullptr);

  // The content should now be gone.
  EXPECT_FALSE(scroll_view_->contents());

  // The contents view should have also been deleted (and therefore the tracker
  // is no longer tracking a view).
  EXPECT_FALSE(tracker.view());
}

// Test scrolling behavior when clicking on the scroll track.
TEST_F(WidgetScrollViewTest, ScrollTrackScrolling) {
  // Set up with a vertical scroller.
  ScrollView* scroll_view =
      AddScrollViewWithContentSize(gfx::Size(10, kDefaultHeight * 5));
  ScrollViewTestApi test_api(scroll_view);
  ScrollBar* scroll_bar = test_api.GetScrollBar(VERTICAL);
  View* thumb = test_api.GetScrollBarThumb(VERTICAL);

  // Click in the middle of the track, ensuring it's below the thumb.
  const gfx::Point location = scroll_bar->bounds().CenterPoint();
  EXPECT_GT(location.y(), thumb->bounds().bottom());
  ui::MouseEvent press(TestLeftMouseAt(location, ui::EventType::kMousePressed));
  ui::MouseEvent release(
      TestLeftMouseAt(location, ui::EventType::kMouseReleased));

  const base::OneShotTimer& timer = test_api.GetScrollBarTimer(VERTICAL);
  EXPECT_FALSE(timer.IsRunning());

  EXPECT_EQ(0, scroll_view->GetVisibleRect().y());
  scroll_bar->OnMouseEvent(&press);

  // Clicking the scroll track should scroll one "page".
  EXPECT_EQ(kDefaultHeight, scroll_view->GetVisibleRect().y());

  // While the mouse is pressed, timer should trigger more scroll events.
  EXPECT_TRUE(timer.IsRunning());

  // Upon release timer should stop (and scroll position should remain).
  scroll_bar->OnMouseEvent(&release);
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_EQ(kDefaultHeight, scroll_view->GetVisibleRect().y());
}

// Test that LocatedEvents are transformed correctly when scrolling.
TEST_F(WidgetScrollViewTest, EventLocation) {
  // Set up with both scrollers.
  auto contents = std::make_unique<CustomView>();
  auto* contents_ptr = contents.get();
  contents->SetPreferredSize(gfx::Size(kDefaultHeight * 5, kDefaultHeight * 5));
  AddScrollViewWithContents(std::move(contents));

  const gfx::Point location_in_widget(10, 10);

  // Click without scrolling.
  TestClickAt(location_in_widget);
  EXPECT_EQ(location_in_widget, contents_ptr->last_location());

  // Scroll down a page.
  contents_ptr->ScrollRectToVisible(
      gfx::Rect(0, kDefaultHeight, 1, kDefaultHeight));
  TestClickAt(location_in_widget);
  EXPECT_EQ(gfx::Point(10, 10 + kDefaultHeight), contents_ptr->last_location());

  // Scroll right a page (and back up).
  contents_ptr->ScrollRectToVisible(
      gfx::Rect(kDefaultWidth, 0, kDefaultWidth, 1));
  TestClickAt(location_in_widget);
  EXPECT_EQ(gfx::Point(10 + kDefaultWidth, 10), contents_ptr->last_location());

  // Scroll both directions.
  contents_ptr->ScrollRectToVisible(
      gfx::Rect(kDefaultWidth, kDefaultHeight, kDefaultWidth, kDefaultHeight));
  TestClickAt(location_in_widget);
  EXPECT_EQ(gfx::Point(10 + kDefaultWidth, 10 + kDefaultHeight),
            contents_ptr->last_location());
}

// Ensure behavior of ScrollRectToVisible() is consistent when scrolling with
// and without layers, and under LTR and RTL.
TEST_P(WidgetScrollViewTestRTLAndLayers, ScrollOffsetWithoutLayers) {
  // Set up with both scrollers. And a nested view hierarchy like:
  // +-------------+
  // |XX           |
  // |  +----------|
  // |  |          |
  // |  |  +-------|
  // |  |  |       |
  // |  |  |  etc. |
  // |  |  |       |
  // +-------------+
  // Note that "XX" indicates the size of the viewport.
  constexpr int kNesting = 5;
  constexpr int kCellWidth = kDefaultWidth;
  constexpr int kCellHeight = kDefaultHeight;
  constexpr gfx::Size kContentSize(kCellWidth * kNesting,
                                   kCellHeight * kNesting);
  ScrollView* scroll_view = AddScrollViewWithContentSize(kContentSize, false);
  ScrollViewTestApi test_api(scroll_view);
  EXPECT_EQ(gfx::PointF(0, 0), test_api.CurrentOffset());

  // Sanity check that the contents has a layer iff testing layers.
  EXPECT_EQ(IsTestingLayers(), !!scroll_view->contents()->layer());

  if (IsTestingRtl()) {
    // Sanity check the hit-testing logic on the root view. That is, verify that
    // coordinates really do flip in RTL. The difference inside the viewport is
    // that the flipping should occur consistently in the entire contents (not
    // just the visible contents), and take into account the scroll offset.
    EXPECT_EQ(gfx::Point(kDefaultWidth - 1, 1),
              HitTestInCorner(scroll_view->GetWidget()->GetRootView(), false));
    EXPECT_EQ(gfx::Point(kContentSize.width() - 1, 1),
              HitTestInCorner(scroll_view->contents(), false));
  } else {
    EXPECT_EQ(gfx::Point(1, 1),
              HitTestInCorner(scroll_view->GetWidget()->GetRootView(), false));
    EXPECT_EQ(gfx::Point(1, 1),
              HitTestInCorner(scroll_view->contents(), false));
  }

  // Test vertical scrolling using coordinates on the contents canvas.
  gfx::Rect offset(0, kCellHeight * 2, kCellWidth, kCellHeight);
  scroll_view->contents()->ScrollRectToVisible(offset);
  EXPECT_EQ(gfx::PointF(0, offset.y()), test_api.CurrentOffset());

  // Rely on auto-flipping for this and future HitTestInCorner() calls.
  EXPECT_EQ(gfx::Point(1, kCellHeight * 2 + 1),
            HitTestInCorner(scroll_view->contents()));

  // Test horizontal scrolling.
  offset.set_x(kCellWidth * 2);
  scroll_view->contents()->ScrollRectToVisible(offset);
  EXPECT_EQ(gfx::PointF(offset.x(), offset.y()), test_api.CurrentOffset());
  EXPECT_EQ(gfx::Point(kCellWidth * 2 + 1, kCellHeight * 2 + 1),
            HitTestInCorner(scroll_view->contents()));

  // Reset the scrolling.
  scroll_view->contents()->ScrollRectToVisible(gfx::Rect(0, 0, 1, 1));

  // Test transformations through a nested view hierarchy.
  View* deepest_view = scroll_view->contents();
  constexpr gfx::Rect kCellRect(kCellWidth, kCellHeight, kContentSize.width(),
                                kContentSize.height());
  for (int i = 1; i < kNesting; ++i) {
    SCOPED_TRACE(testing::Message("Nesting = ") << i);
    View* child = new View;
    child->SetBoundsRect(kCellRect);
    deepest_view->AddChildView(child);
    deepest_view = child;

    // Add a view in one quadrant. Scrolling just this view should only scroll
    // far enough for it to become visible. That is, it should be positioned at
    // the bottom right of the viewport, not the top-left. But since there are
    // scroll bars, the scroll offset needs to go "a bit more".
    View* partial_view = new View;
    partial_view->SetSize(gfx::Size(kCellWidth / 3, kCellHeight / 3));
    deepest_view->AddChildView(partial_view);
    partial_view->ScrollViewToVisible();
    int x_offset_in_cell = kCellWidth - partial_view->width();
    if (!scroll_view->horizontal_scroll_bar()->OverlapsContent())
      x_offset_in_cell -= scroll_view->horizontal_scroll_bar()->GetThickness();
    int y_offset_in_cell = kCellHeight - partial_view->height();
    if (!scroll_view->vertical_scroll_bar()->OverlapsContent())
      y_offset_in_cell -= scroll_view->vertical_scroll_bar()->GetThickness();
    EXPECT_EQ(gfx::PointF(kCellWidth * i - x_offset_in_cell,
                          kCellHeight * i - y_offset_in_cell),
              test_api.CurrentOffset());

    // Now scroll the rest.
    deepest_view->ScrollViewToVisible();
    EXPECT_EQ(gfx::PointF(kCellWidth * i, kCellHeight * i),
              test_api.CurrentOffset());

    // The partial view should now be at the top-left of the viewport (top-right
    // in RTL).
    EXPECT_EQ(gfx::Point(1, 1), HitTestInCorner(partial_view));

    gfx::Point origin;
    View::ConvertPointToWidget(partial_view, &origin);
    constexpr gfx::Point kTestPointRTL(kDefaultWidth - kCellWidth / 3, 0);
    EXPECT_EQ(IsTestingRtl() ? kTestPointRTL : gfx::Point(), origin);
  }

  // Scrolling to the deepest view should have moved the viewport so that the
  // (kNesting - 1) parent views are all off-screen.
  EXPECT_EQ(
      gfx::PointF(kCellWidth * (kNesting - 1), kCellHeight * (kNesting - 1)),
      test_api.CurrentOffset());
}

// Test that views scroll offsets are in sync with the layer scroll offsets.
TEST_P(WidgetScrollViewTestRTLAndLayers, ScrollOffsetUsingLayers) {
  // Set up with both scrollers, but don't commit the layer changes yet.
  ScrollView* scroll_view = AddScrollViewWithContentSize(
      gfx::Size(kDefaultWidth * 5, kDefaultHeight * 5), false);
  ScrollViewTestApi test_api(scroll_view);

  EXPECT_EQ(gfx::PointF(0, 0), test_api.CurrentOffset());

  // UI code may request a scroll before layer changes are committed.
  gfx::Rect offset(0, kDefaultHeight * 2, kDefaultWidth, kDefaultHeight);
  scroll_view->contents()->ScrollRectToVisible(offset);
  EXPECT_EQ(gfx::PointF(0, offset.y()), test_api.CurrentOffset());

  // The following only makes sense when layered scrolling is enabled.
  View* container = scroll_view->contents();
  EXPECT_EQ(IsTestingLayers(), !!container->layer());
  if (!container->layer())
    return;

  // Container and viewport should have layers.
  EXPECT_TRUE(container->layer());
  EXPECT_TRUE(test_api.contents_viewport()->layer());

  // In a Widget, so there should be a compositor.
  ui::Compositor* compositor = container->layer()->GetCompositor();
  EXPECT_TRUE(compositor);

  // But setting on the impl side should fail since the layer isn't committed.
  cc::ElementId element_id =
      container->layer()->cc_layer_for_testing()->element_id();
  EXPECT_FALSE(compositor->ScrollLayerTo(element_id, gfx::PointF(0, 0)));
  EXPECT_EQ(gfx::PointF(0, offset.y()), test_api.CurrentOffset());

  WaitForCommit();
  EXPECT_EQ(gfx::PointF(0, offset.y()), test_api.CurrentOffset());

  // Upon commit, the impl side should report the same value too.
  gfx::PointF impl_offset;
  EXPECT_TRUE(compositor->GetScrollOffsetForLayer(element_id, &impl_offset));
  EXPECT_EQ(gfx::PointF(0, offset.y()), impl_offset);

  // Now impl-side scrolling should work, and also update the ScrollView.
  offset.set_y(kDefaultHeight * 3);
  EXPECT_TRUE(
      compositor->ScrollLayerTo(element_id, gfx::PointF(0, offset.y())));
  EXPECT_EQ(gfx::PointF(0, offset.y()), test_api.CurrentOffset());

  // Scroll via ScrollView API. Should be reflected on the impl side.
  offset.set_y(kDefaultHeight * 4);
  scroll_view->contents()->ScrollRectToVisible(offset);
  EXPECT_EQ(gfx::PointF(0, offset.y()), test_api.CurrentOffset());

  EXPECT_TRUE(compositor->GetScrollOffsetForLayer(element_id, &impl_offset));
  EXPECT_EQ(gfx::PointF(0, offset.y()), impl_offset);

  // Test horizontal scrolling.
  offset.set_x(kDefaultWidth * 2);
  scroll_view->contents()->ScrollRectToVisible(offset);
  EXPECT_EQ(gfx::PointF(offset.x(), offset.y()), test_api.CurrentOffset());

  EXPECT_TRUE(compositor->GetScrollOffsetForLayer(element_id, &impl_offset));
  EXPECT_EQ(gfx::PointF(offset.x(), offset.y()), impl_offset);
}

namespace {

// Applies |scroll_event| to |scroll_view| and verifies that the event is
// applied correctly whether or not compositor scrolling is enabled.
static void ApplyScrollEvent(const ScrollViewTestApi& test_api,
                             ScrollView* scroll_view,
                             ui::ScrollEvent& scroll_event) {
  EXPECT_FALSE(scroll_event.handled());
  EXPECT_FALSE(scroll_event.stopped_propagation());
  scroll_view->OnScrollEvent(&scroll_event);

  // Check to see if the scroll event is handled by the scroll view.
  if (base::FeatureList::IsEnabled(::features::kUiCompositorScrollWithLayers)) {
    // If UiCompositorScrollWithLayers is enabled, the event is set handled
    // and its propagation is stopped.
    EXPECT_TRUE(scroll_event.handled());
    EXPECT_TRUE(scroll_event.stopped_propagation());
  } else {
    // If UiCompositorScrollWithLayers is disabled, the event isn't handled.
    // This informs Widget::OnScrollEvent() to convert to a MouseWheel event
    // and dispatch again. Simulate that.
    EXPECT_FALSE(scroll_event.handled());
    EXPECT_FALSE(scroll_event.stopped_propagation());
    EXPECT_EQ(gfx::PointF(), test_api.CurrentOffset());

    ui::MouseWheelEvent wheel(scroll_event);
    scroll_view->OnMouseEvent(&wheel);
  }
}

}  // namespace

// Tests to see the scroll events are handled correctly in composited and
// non-composited scrolling.
TEST_F(WidgetScrollViewTest, CompositedScrollEvents) {
  // Set up with a vertical scroll bar.
  ScrollView* scroll_view =
      AddScrollViewWithContentSize(gfx::Size(10, kDefaultHeight * 5));
  ScrollViewTestApi test_api(scroll_view);

  // Create a fake scroll event and send it to the scroll view.
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(),
                         base::TimeTicks::Now(), 0, 0, -10, 0, -10, 3);
  ApplyScrollEvent(test_api, scroll_view, scroll);

  // Check if the scroll view has been offset.
  EXPECT_EQ(gfx::PointF(0, 10), test_api.CurrentOffset());
}

// Tests to see that transposed (treat-as-horizontal) scroll events are handled
// correctly in composited and non-composited scrolling.
TEST_F(WidgetScrollViewTest, CompositedTransposedScrollEvents) {
  // Set up with a vertical scroll bar.
  ScrollView* scroll_view =
      AddScrollViewWithContentSize(gfx::Size(kDefaultHeight * 5, 10));
  scroll_view->SetTreatAllScrollEventsAsHorizontal(true);
  ScrollViewTestApi test_api(scroll_view);

  // Create a fake scroll event and send it to the scroll view.
  // Note that this is still a VERTICAL scroll event, but we'll be looking for
  // HORIZONTAL motion later because we're transposed.
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(),
                         base::TimeTicks::Now(), 0, 0, -10, 0, -10, 3);
  ApplyScrollEvent(test_api, scroll_view, scroll);

  // Check if the scroll view has been offset.
  EXPECT_EQ(gfx::PointF(10, 0), test_api.CurrentOffset());
}

// Tests to see that transposed (treat-as-horizontal) scroll events are handled
// correctly in composited and non-composited scrolling when the scroll offset
// is somewhat ambiguous. This is the case where the horizontal component is
// larger than the vertical.
TEST_F(WidgetScrollViewTest,
       DISABLED_CompositedTransposedScrollEventsHorizontalComponentIsLarger) {
  // Set up with a vertical scroll bar.
  ScrollView* scroll_view =
      AddScrollViewWithContentSize(gfx::Size(kDefaultHeight * 5, 10));
  scroll_view->SetTreatAllScrollEventsAsHorizontal(true);
  ScrollViewTestApi test_api(scroll_view);

  // Create a fake scroll event and send it to the scroll view.
  // This will be a horizontal scroll event but there will be a conflicting
  // vertical element. We should still scroll horizontally, since the horizontal
  // component is greater.
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(),
                         base::TimeTicks::Now(), 0, -10, 7, -10, 7, 3);
  ApplyScrollEvent(test_api, scroll_view, scroll);

  // Check if the scroll view has been offset.
  EXPECT_EQ(gfx::PointF(10, 0), test_api.CurrentOffset());
}

// Tests to see that transposed (treat-as-horizontal) scroll events are handled
// correctly in composited and non-composited scrolling when the scroll offset
// is somewhat ambiguous. This is the case where the vertical component is
// larger than the horizontal.
TEST_F(WidgetScrollViewTest,
       CompositedTransposedScrollEventsVerticalComponentIsLarger) {
  // Set up with a vertical scroll bar.
  ScrollView* scroll_view =
      AddScrollViewWithContentSize(gfx::Size(kDefaultHeight * 5, 10));
  scroll_view->SetTreatAllScrollEventsAsHorizontal(true);
  ScrollViewTestApi test_api(scroll_view);

  // Create a fake scroll event and send it to the scroll view.
  // This will be a vertical scroll event but there will be a conflicting
  // horizontal element. We should still scroll horizontally, since the vertical
  // component is greater.
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(),
                         base::TimeTicks::Now(), 0, 7, -10, 7, -10, 3);
  ApplyScrollEvent(test_api, scroll_view, scroll);

  // Check if the scroll view has been offset.
  EXPECT_EQ(gfx::PointF(10, 0), test_api.CurrentOffset());
}

TEST_F(WidgetScrollViewTest, UnboundedScrollViewUsesContentPreferredSize) {
  auto contents = std::make_unique<View>();
  constexpr gfx::Size kContentsPreferredSize(500, 500);
  contents->SetPreferredSize(kContentsPreferredSize);
  ScrollView* scroll_view =
      AddScrollViewWithContents(std::move(contents), true);
  EXPECT_EQ(kContentsPreferredSize, scroll_view->GetPreferredSize({}));

  constexpr gfx::Insets kInsets(20);
  scroll_view->SetBorder(CreateEmptyBorder(kInsets));
  gfx::Size preferred_size_with_insets(kContentsPreferredSize);
  preferred_size_with_insets.Enlarge(kInsets.width(), kInsets.height());
  EXPECT_EQ(preferred_size_with_insets, scroll_view->GetPreferredSize({}));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WidgetScrollViewTestRTLAndLayers,
                         ::testing::Values(UiConfig::kLtr,
                                           UiConfig::kRtl,
                                           UiConfig::kLtrWithLayers,
                                           UiConfig::kRtlWithLayers),
                         &UiConfigToString);

}  // namespace views
