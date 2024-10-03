// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/host_begin_frame_observer.h"

#include "base/logging.h"
#include "base/task/common/task_annotator.h"
#include "base/time/time.h"

namespace ui {

HostBeginFrameObserver::HostBeginFrameObserver(
    SimpleBeginFrameObserverList& observers,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : simple_begin_frame_observers_(observers),
      task_runner_(std::move(task_runner)) {}

HostBeginFrameObserver::~HostBeginFrameObserver() = default;

void HostBeginFrameObserver::OnStandaloneBeginFrame(
    const viz::BeginFrameArgs& args) {
  // Mark the current task as interesting, as it maybe be responsible for
  // handling input events for flings.
  base::TaskAnnotator::MarkCurrentTaskAsInterestingForTracing();
  if (args.type == viz::BeginFrameArgs::MISSED) {
    return;
  }

  if (pending_coalesce_callback_) {
    begin_frame_args_ = args;
    return;
  }

  if ((base::TimeTicks::Now() - args.frame_time) > args.interval) {
    begin_frame_args_ = args;
    pending_coalesce_callback_ = true;
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HostBeginFrameObserver::CoalescedBeginFrame,
                       weak_factory_.GetWeakPtr()),
        base::Microseconds(1));
    return;
  }

  CallObservers(args);
}

mojo::PendingRemote<viz::mojom::BeginFrameObserver>
HostBeginFrameObserver::GetBoundRemote() {
  return receiver_.BindNewPipeAndPassRemote(task_runner_);
}

void HostBeginFrameObserver::CoalescedBeginFrame() {
  DCHECK(begin_frame_args_.IsValid());
  pending_coalesce_callback_ = false;
  viz::BeginFrameArgs args = begin_frame_args_;
  begin_frame_args_ = viz::BeginFrameArgs();
  CallObservers(args);
}

// This may be deleted as part of `CallObservers`.
void HostBeginFrameObserver::CallObservers(const viz::BeginFrameArgs& args) {
  simple_begin_frame_observers_->Notify(&SimpleBeginFrameObserver::OnBeginFrame,
                                        args.frame_time, args.interval);
}

}  // namespace ui
