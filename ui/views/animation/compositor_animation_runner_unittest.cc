// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/compositor_animation_runner.h"

#include "base/test/bind_test_util.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/widget_test.h"

namespace views {
namespace test {
namespace {
constexpr base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(100);
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
}

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

}  // namespace test
}  // namespace views
