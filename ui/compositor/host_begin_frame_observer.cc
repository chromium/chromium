// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/host_begin_frame_observer.h"

#include <optional>

#include "base/logging.h"
#include "base/task/common/task_annotator.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace ui {
namespace {

void WriteBeginFrameIdToTrace(perfetto::EventContext& ctx,
                              const viz::BeginFrameId frame_id) {
  auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
  auto* begin_frame_id = event->set_begin_frame_id();
  begin_frame_id->set_source_id(frame_id.source_id);
  begin_frame_id->set_sequence_number(frame_id.sequence_number);
}

}  // namespace

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
    TRACE_EVENT_INSTANT("ui", "HostBeginFrameObserver::continue coalescing",
                        perfetto::Flow::Global(coalesce_flow_id_),
                        [&](perfetto::EventContext ctx) {
                          WriteBeginFrameIdToTrace(ctx, args.frame_id);
                        });
    if (!first_coalesced_begin_frame_time_) {
      first_coalesced_begin_frame_time_ = begin_frame_args_.frame_time;
    }
    begin_frame_args_ = args;
    return;
  }

  if ((base::TimeTicks::Now() - args.frame_time) > args.interval) {
    begin_frame_args_ = args;
    pending_coalesce_callback_ = true;
    coalesce_flow_id_ = base::trace_event::GetNextGlobalTraceId();
    CHECK(!first_coalesced_begin_frame_time_);
    TRACE_EVENT_INSTANT("ui", "HostBeginFrameObserver::start coalescing",
                        perfetto::Flow::Global(coalesce_flow_id_),
                        [&](perfetto::EventContext ctx) {
                          WriteBeginFrameIdToTrace(ctx, args.frame_id);
                        });
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HostBeginFrameObserver::CoalescedBeginFrame,
                       weak_factory_.GetWeakPtr()),
        base::Microseconds(1));
    return;
  }

  CallObservers(args, std::nullopt);
}

mojo::PendingRemote<viz::mojom::BeginFrameObserver>
HostBeginFrameObserver::GetBoundRemote() {
  return receiver_.BindNewPipeAndPassRemote(task_runner_);
}

void HostBeginFrameObserver::CoalescedBeginFrame() {
  DCHECK(begin_frame_args_.IsValid());
  TRACE_EVENT_INSTANT("ui", "HostBeginFrameObserver::finish coalescing",
                      perfetto::Flow::Global(coalesce_flow_id_),
                      [&](perfetto::EventContext ctx) {
                        WriteBeginFrameIdToTrace(ctx,
                                                 begin_frame_args_.frame_id);
                      });
  pending_coalesce_callback_ = false;
  viz::BeginFrameArgs args = begin_frame_args_;
  begin_frame_args_ = viz::BeginFrameArgs();
  std::optional<base::TimeTicks> first_coalesced_begin_frame_time =
      first_coalesced_begin_frame_time_;
  first_coalesced_begin_frame_time_ = std::nullopt;
  coalesce_flow_id_ = ~0ull;
  CallObservers(args, first_coalesced_begin_frame_time);
}

// This may be deleted as part of `CallObservers`.
void HostBeginFrameObserver::CallObservers(
    const viz::BeginFrameArgs& args,
    std::optional<const base::TimeTicks> first_coalesced_begin_frame_time) {
  simple_begin_frame_observers_->Notify(&SimpleBeginFrameObserver::OnBeginFrame,
                                        args.frame_time, args.interval,
                                        first_coalesced_begin_frame_time);
}

}  // namespace ui
