// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/scrollbar_animator_mac.h"

#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace ui {

namespace {

constexpr int kExpandedWidth = 32;
constexpr int kUnexpandedWidth = 16;
constexpr auto kTimeToFadeOut = base::Milliseconds(500);
constexpr auto kAnimationTime = base::Milliseconds(250);

class MockClient : public OverlayScrollbarAnimatorMac::Client {
 public:
  MOCK_CONST_METHOD0(IsMouseInScrollbarFrameRect, bool());
  MOCK_METHOD1(SetHidden, void(bool));
  MOCK_METHOD0(SetThumbNeedsDisplay, void());
  MOCK_METHOD0(SetTrackNeedsDisplay, void());
};

class MacScrollbarAnimatorTest : public ::testing::Test {
 public:
  MacScrollbarAnimatorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scrollbar_ = std::make_unique<OverlayScrollbarAnimatorMac>(
        &client_, kExpandedWidth, kUnexpandedWidth,
        task_environment_.GetMainThreadTaskRunner());
  }

  ~MacScrollbarAnimatorTest() override = default;

  base::test::TaskEnvironment task_environment_;
  MockClient client_;
  std::unique_ptr<OverlayScrollbarAnimatorMac> scrollbar_;
};

TEST_F(MacScrollbarAnimatorTest, DidScrollThenFadeOut) {
  // The scrollbar_ starts as invisible.
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);

  // Scroll with the mouse not in the track.
  EXPECT_CALL(client_, IsMouseInScrollbarFrameRect()).WillOnce(Return(false));
  EXPECT_CALL(client_, SetHidden(false));
  EXPECT_CALL(client_, SetThumbNeedsDisplay());
  scrollbar_->DidScroll();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kUnexpandedWidth);

  // Fast-forward until just before the timeout threshold is hit.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).Times(0);
  task_environment_.FastForwardBy(kTimeToFadeOut);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);

  // Fast-forward through half of the fade-out animation.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).WillRepeatedly(Return());
  task_environment_.FastForwardBy(kAnimationTime / 2);
  EXPECT_GT(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kUnexpandedWidth);

  // Fast-forward through past the end of the fade-out animation. The scrollbar_
  // should be hidden.
  EXPECT_CALL(client_, SetHidden(true));
  task_environment_.FastForwardBy(kAnimationTime);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);
}

TEST_F(MacScrollbarAnimatorTest, DidScrollStartsInTrack) {
  // Scroll with the mouse in the track.
  EXPECT_CALL(client_, IsMouseInScrollbarFrameRect()).WillOnce(Return(true));
  EXPECT_CALL(client_, SetHidden(false));
  EXPECT_CALL(client_, SetThumbNeedsDisplay());
  EXPECT_CALL(client_, SetTrackNeedsDisplay());
  scrollbar_->DidScroll();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Fast-forward long past the fade-out animation would happen. Nothing should
  // happen.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).Times(0);
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).Times(0);
  task_environment_.FastForwardBy(10 * kTimeToFadeOut);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Scroll again. Again, nothing should happen.
  scrollbar_->DidScroll();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Have the mouse leave the scrollbar_. Fast-forward not enough time for the
  // scrollbar_ to fade out, then have the mouse re-enter. Exit again. Still
  // nothing should happen.
  scrollbar_->MouseDidExit();
  task_environment_.FastForwardBy(kTimeToFadeOut / 2);
  scrollbar_->MouseDidEnter();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);
  scrollbar_->MouseDidExit();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Wait most of the fade-out timer's time. Nothing should happen in this
  // interval.
  task_environment_.FastForwardBy(3 * kTimeToFadeOut / 4);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Allow the fade-out animation to progress about halfway through.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).WillRepeatedly(Return());
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).WillRepeatedly(Return());
  task_environment_.FastForwardBy(kTimeToFadeOut / 4 + kAnimationTime / 2);
  EXPECT_GT(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_GT(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Scroll, interrupting the fade-out animation.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).Times(1);
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).Times(1);
  scrollbar_->DidScroll();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Wait most of the fade-out timer's time. Nothing should happen in this time.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).Times(0);
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).Times(0);
  task_environment_.FastForwardBy(3 * kTimeToFadeOut / 4);

  // Allow the fade-out animation to progress halfway again.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).WillRepeatedly(Return());
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).WillRepeatedly(Return());
  task_environment_.FastForwardBy(kTimeToFadeOut / 4 + kAnimationTime / 2);
  EXPECT_GT(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_GT(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // And let it progress to completion. The scrollbar_ should hide itself.
  EXPECT_CALL(client_, SetHidden(true));
  task_environment_.FastForwardBy(kAnimationTime);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kUnexpandedWidth);
}

TEST_F(MacScrollbarAnimatorTest, EnterTrack) {
  // First try to enter the track before showing the scrollbar_. Nothing should
  // happen.
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);
  scrollbar_->MouseDidEnter();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);

  // Scroll with the mouse in the track.
  EXPECT_CALL(client_, IsMouseInScrollbarFrameRect()).WillOnce(Return(false));
  EXPECT_CALL(client_, SetHidden(false));
  EXPECT_CALL(client_, SetThumbNeedsDisplay());
  scrollbar_->DidScroll();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kUnexpandedWidth);

  // Have the mouse enter the scrollbar_ area.
  scrollbar_->MouseDidEnter();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kUnexpandedWidth);

  // Fast-forward part of the animation.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).WillRepeatedly(Return());
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).WillRepeatedly(Return());
  task_environment_.FastForwardBy(kAnimationTime / 2);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_GT(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_GT(scrollbar_->GetThumbWidth(), kUnexpandedWidth);
  EXPECT_LT(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Fast-forward through the rest of the animation.
  task_environment_.FastForwardBy(kAnimationTime);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Fast-forward for a long time. Nothing should happen (no fade-out).
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).Times(0);
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).Times(0);
  task_environment_.FastForwardBy(10 * kTimeToFadeOut);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Move the mouse out and wait for the the fade-out should start.
  scrollbar_->MouseDidExit();
  task_environment_.FastForwardBy(kTimeToFadeOut);
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).WillRepeatedly(Return());
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).WillRepeatedly(Return());
  task_environment_.FastForwardBy(kAnimationTime / 2);
  EXPECT_GT(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_GT(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Re-enter. This will immediately return us to full opacity.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).Times(1);
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).Times(1);
  scrollbar_->MouseDidEnter();
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Wait for a long time. Nothing should happen.
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).Times(0);
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).Times(0);
  task_environment_.FastForwardBy(10 * kTimeToFadeOut);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Re-leave the track. Again nothing should happen.
  scrollbar_->MouseDidExit();

  // Fast-forward through the start of the fade-out.
  task_environment_.FastForwardBy(kTimeToFadeOut);
  EXPECT_CALL(client_, SetThumbNeedsDisplay()).WillRepeatedly(Return());
  EXPECT_CALL(client_, SetTrackNeedsDisplay()).WillRepeatedly(Return());
  task_environment_.FastForwardBy(kAnimationTime / 2);
  EXPECT_GT(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetThumbAlpha(), 1.f);
  EXPECT_GT(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_LT(scrollbar_->GetTrackAlpha(), 1.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kExpandedWidth);

  // Fast-forward until the fade-out completes.
  EXPECT_CALL(client_, SetHidden(true));
  task_environment_.FastForwardBy(kAnimationTime);
  EXPECT_EQ(scrollbar_->GetThumbAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetTrackAlpha(), 0.f);
  EXPECT_EQ(scrollbar_->GetThumbWidth(), kUnexpandedWidth);
}

}  // namespace

}  // namespace ui
