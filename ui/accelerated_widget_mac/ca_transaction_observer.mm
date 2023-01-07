// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_transaction_observer.h"

#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <QuartzCore/QuartzCore.h>

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
NSString* kRunLoopMode = @"Chrome CATransactionCoordinator commit handler";
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
    base::TimeTicks deadline = start_time;
    for (auto& observer : pre_commit_observers_) {
      if (observer.ShouldWaitInPreCommit()) {
        continue_waiting = true;
        deadline = std::max(deadline, start_time + observer.PreCommitTimeout());
      }
    }
    if (!continue_waiting)
      break;  // success

    base::TimeDelta time_left = deadline - clock->NowTicks();
    if (time_left <= base::Seconds(0))
      break;  // timeout

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
    bool continue_waiting = base::ranges::any_of(
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

CATransactionCoordinator::CATransactionCoordinator() = default;
CATransactionCoordinator::~CATransactionCoordinator() = default;

void CATransactionCoordinator::Synchronize() {
  if (disabled_for_testing_)
    return;
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

}  // namespace ui
