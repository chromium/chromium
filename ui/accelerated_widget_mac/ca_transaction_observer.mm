// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_transaction_observer.h"

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_features.h"

typedef NS_ENUM(unsigned int, CATransactionPhase) {
  kCATransactionPhasePreLayout,
  kCATransactionPhasePreCommit,
  kCATransactionPhasePostCommit,
};

@interface CATransaction ()
+ (void)addCommitHandler:(void (^)(void))block
                forPhase:(CATransactionPhase)phase;
@end

namespace ui {

namespace {
NSString* const kRunLoopMode =
    @"Chrome CATransactionCoordinator commit handler";
constexpr auto kPostCommitTimeout = base::Milliseconds(50);
}  // namespace

CATransactionCoordinator& CATransactionCoordinator::Get() {
  static base::NoDestructor<CATransactionCoordinator> instance;
  return *instance;
}

void CATransactionCoordinator::SynchronizeImpl() {
  static bool registeredRunLoopMode = false;
  if (!registeredRunLoopMode) {
    CFRunLoopAddCommonMode(CFRunLoopGetCurrent(),
                           static_cast<CFStringRef>(kRunLoopMode));
    registeredRunLoopMode = true;
  }
  if (active_)
    return;
  active_ = true;

  for (auto& observer : post_commit_observers_)
    observer->OnActivateForTransaction();

  [CATransaction addCommitHandler:^{
    PreCommitHandler();
  }
                         forPhase:kCATransactionPhasePreCommit];

  [CATransaction addCommitHandler:^{
    PostCommitHandler();
  }
                         forPhase:kCATransactionPhasePostCommit];
}

void CATransactionCoordinator::PreCommitHandler() {
  TRACE_EVENT0("ui", "CATransactionCoordinator: pre-commit handler");
  auto* clock = base::DefaultTickClock::GetInstance();
  const base::TimeTicks start_time = clock->NowTicks();
  while (true) {
    bool continue_waiting = false;
    bool any_observed_window_in_live_resize = false;
    base::TimeTicks deadline = start_time;
    for (auto& observer : pre_commit_observers_) {
      if (observer.ShouldWaitInPreCommit()) {
        continue_waiting = true;
        deadline = std::max(deadline, start_time + observer.PreCommitTimeout());
        if (observer.IsWindowInLiveResize()) {
          any_observed_window_in_live_resize = true;
        }
      }
    }
    if (!continue_waiting)
      break;  // success

    base::TimeDelta time_left = deadline - clock->NowTicks();
    if (time_left <= base::Seconds(0))
      break;  // timeout

    // If there is no live resize there is still a need to invoke
    // `WaitForSingleTaskToRun()` in case there are already queued tasks.
    // There should not be any waiting for additional tasks so the waiting is
    // kept to essentially zero.
    if (base::FeatureList::IsEnabled(
            features::kOnlyUseWindowResizeHelperOnResize) &&
        !any_observed_window_in_live_resize) {
      time_left = base::Nanoseconds(1);
    }

    ui::WindowResizeHelperMac::Get()->WaitForSingleTaskToRun(time_left);
  }
}

void CATransactionCoordinator::PostCommitHandler() {
  TRACE_EVENT0("ui", "CATransactionCoordinator: post-commit handler");

  for (auto& observer : post_commit_observers_)
    observer->OnEnterPostCommit();

  auto* clock = base::DefaultTickClock::GetInstance();
  const base::TimeTicks deadline = clock->NowTicks() + kPostCommitTimeout;
  while (true) {
    bool continue_waiting = std::ranges::any_of(
        post_commit_observers_, &PostCommitObserver::ShouldWaitInPostCommit);
    if (!continue_waiting)
      break;  // success

    base::TimeDelta time_left = deadline - clock->NowTicks();
    if (time_left <= base::Seconds(0))
      break;  // timeout

    ui::WindowResizeHelperMac::Get()->WaitForSingleTaskToRun(time_left);
  }
  active_ = false;
}

CATransactionCoordinator::CATransactionCoordinator() {
  if (base::FeatureList::IsEnabled(features::kCATransactionV2)) {
    base::CurrentThread::Get()->AddTaskObserver(this);
  }
}

CATransactionCoordinator::~CATransactionCoordinator() {
  if (base::FeatureList::IsEnabled(features::kCATransactionV2)) {
    base::CurrentThread::Get()->RemoveTaskObserver(this);
  }
}

void CATransactionCoordinator::Synchronize() {
  if (disabled_for_testing_)
    return;

  if (base::FeatureList::IsEnabled(features::kCATransactionV2)) {
    if (!ca_action_disabler_) {
      ca_action_disabler_ = std::make_unique<ScopedCAActionDisabler>();
    }
    return;
  }

  SynchronizeImpl();
}

void CATransactionCoordinator::AddPreCommitObserver(
    PreCommitObserver* observer) {
  pre_commit_observers_.AddObserver(observer);
}

void CATransactionCoordinator::RemovePreCommitObserver(
    PreCommitObserver* observer) {
  pre_commit_observers_.RemoveObserver(observer);
}

void CATransactionCoordinator::AddPostCommitObserver(
    scoped_refptr<PostCommitObserver> observer) {
  DCHECK(!post_commit_observers_.count(observer));
  post_commit_observers_.insert(std::move(observer));
}

void CATransactionCoordinator::RemovePostCommitObserver(
    scoped_refptr<PostCommitObserver> observer) {
  DCHECK(post_commit_observers_.count(observer));
  post_commit_observers_.erase(std::move(observer));
}

void CATransactionCoordinator::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {}

void CATransactionCoordinator::DidProcessTask(
    const base::PendingTask& pending_task) {
  if (!ca_action_disabler_) {
    return;
  }

  // The PreCommitHandler is what waits for compositor frames to arrive.
  // TODO(https://crbug.com/507113013): Rename this after the non-V2 path is
  // removed.
  PreCommitHandler();

  // Release the ScopedCAActionDisabler only after the new frame has been
  // displayed. Releasing after PreCommitHandler also ensures that recursive
  // calls to PreCommitHandler do not happen.
  ca_action_disabler_.reset();
}

}  // namespace ui
