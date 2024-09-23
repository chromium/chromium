// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/compositor_animation_runner.h"

#include "base/test/bind.h"
#include "base/timer/timer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/widget_test.h"

namespace views::test {
namespace {
constexpr base::TimeDelta kDuration = base::Milliseconds(100);
}

using CompositorAnimationRunnerTest = WidgetTest;

TEST_F(CompositorAnimationRunnerTest, BasicCoverageTest) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->Show();

  AnimationDelegateViews delegate(widget->GetContentsView());
  gfx::LinearAnimation animation(
      kDuration, gfx::LinearAnimation::kDefaultFrameRate, &delegate);

  base::RepeatingTimer interval_timer;
  base::RunLoop run_loop;

  animation.Start();
  EXPECT_TRUE(animation.is_animating());
  EXPECT_TRUE(delegate.container()->has_custom_animation_runner());

  interval_timer.Start(FROM_HERE, kDuration, base::BindLambdaForTesting([&]() {
                         if (animation.is_animating())
                           return;

                         interval_timer.Stop();
                         run_loop.Quit();
                       }));

  run_loop.Run();

  // Verifies that AnimationDelegateViews carries location of call sites
  // instead of implementation.
  EXPECT_STREQ(base::Location::Current().file_name(),
               delegate.location_for_test().file_name());
}

namespace {

// Test AnimationDelegateView which has a non-zero expected animation duration
// time, which is required for getting smoothness reports.
class TestAnimationDelegateViews : public AnimationDelegateViews {
 public:
  explicit TestAnimationDelegateViews(View* view)
      : AnimationDelegateViews(view) {}
  TestAnimationDelegateViews(TestAnimationDelegateViews&) = delete;
  TestAnimationDelegateViews& operator=(TestAnimationDelegateViews&) = delete;
  ~TestAnimationDelegateViews() override = default;

  // AnimationDelegateViews:
  base::TimeDelta GetAnimationDurationForReporting() const override {
    return kDuration;
  }
};

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
// Tests that ui::ThroughputTracker will report for gfx::Animation. Only
// supported on ChromeOS.
TEST_F(CompositorAnimationRunnerTest, ThroughputTracker) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->Show();

  ui::DrawWaiterForTest::WaitForCompositingStarted(widget->GetCompositor());

  int report_count = 0;
  int report_count2 = 0;

  TestAnimationDelegateViews delegate(widget->GetContentsView());

  gfx::LinearAnimation animation(
      kDuration, gfx::LinearAnimation::kDefaultFrameRate, &delegate);

  base::RepeatingTimer interval_timer;
  base::RunLoop run_loop;

  ui::ThroughputTracker tracker1 =
      widget->GetCompositor()->RequestNewThroughputTracker();
  tracker1.Start(base::BindLambdaForTesting(
      [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
        ++report_count;
        run_loop.Quit();
      }));

  animation.Start();
  EXPECT_TRUE(animation.is_animating());
  EXPECT_TRUE(delegate.container()->has_custom_animation_runner());

  interval_timer.Start(FROM_HERE, kDuration, base::BindLambdaForTesting([&]() {
                         if (animation.is_animating())
                           return;

                         interval_timer.Stop();
                         tracker1.Stop();
                       }));
  run_loop.Run();
  EXPECT_EQ(1, report_count);
  EXPECT_EQ(0, report_count2);

  // Tests that switching metrics reporters for the next animation works as
  // expected.
  base::RunLoop run_loop2;

  ui::ThroughputTracker tracker2 =
      widget->GetCompositor()->RequestNewThroughputTracker();
  tracker2.Start(base::BindLambdaForTesting(
      [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
        ++report_count2;
        run_loop2.Quit();
      }));

  animation.Start();
  EXPECT_TRUE(animation.is_animating());

  interval_timer.Start(FROM_HERE, kDuration, base::BindLambdaForTesting([&]() {
                         if (animation.is_animating())
                           return;

                         interval_timer.Stop();
                         tracker2.Stop();
                       }));
  run_loop2.Run();
  EXPECT_EQ(1, report_count);
  EXPECT_EQ(1, report_count2);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// No DesktopAura on ChromeOS.
// Each widget on MACOSX has its own ui::Compositor.
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
using CompositorAnimationRunnerDesktopTest = DesktopWidgetTest;

TEST_F(CompositorAnimationRunnerDesktopTest, SwitchCompositor) {
  WidgetAutoclosePtr widget1(CreateTopLevelNativeWidget());
  widget1->Show();

  WidgetAutoclosePtr widget2(CreateTopLevelNativeWidget());
  widget2->Show();

  ASSERT_NE(widget1->GetCompositor(), widget2->GetCompositor());

  Widget* child = CreateChildNativeWidgetWithParent(widget1.get());
  child->Show();
  AnimationDelegateViews delegate(child->GetContentsView());
  gfx::LinearAnimation animation(
      kDuration, gfx::LinearAnimation::kDefaultFrameRate, &delegate);

  base::RepeatingTimer interval_timer;

  animation.Start();
  EXPECT_TRUE(animation.is_animating());
  EXPECT_TRUE(delegate.container()->has_custom_animation_runner());
  {
    base::RunLoop run_loop;
    interval_timer.Start(FROM_HERE, kDuration,
                         base::BindLambdaForTesting([&]() {
                           if (animation.is_animating())
                             return;
                           interval_timer.Stop();
                           run_loop.Quit();
                         }));
    run_loop.Run();
  }

  EXPECT_FALSE(animation.is_animating());

  Widget::ReparentNativeView(child->GetNativeView(), widget2->GetNativeView());
  widget1.reset();

  animation.Start();
  EXPECT_TRUE(animation.is_animating());
  EXPECT_TRUE(delegate.container()->has_custom_animation_runner());

  {
    base::RunLoop run_loop;
    interval_timer.Start(FROM_HERE, kDuration,
                         base::BindLambdaForTesting([&]() {
                           if (animation.is_animating())
                             return;

                           interval_timer.Stop();
                           run_loop.Quit();
                         }));

    run_loop.Run();
  }
}
#endif

}  // namespace views::test
