// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/external_begin_frame_adapter.h"

#include "base/atomic_sequence_num.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"

namespace ui {

// static
uint64_t ExternalBeginFrameAdapter::AllocateSourceId() {
  // Start at 1 to avoid colliding with the default original_source_id_ (0)
  // in ExternalBeginFrameSourceMojo.
  static base::AtomicSequenceNumber counter;
  return counter.GetNext() + 1;
}

ExternalBeginFrameAdapter::ExternalBeginFrameAdapter(
    Compositor* compositor,
    BeginFrameSourceExtension* source)
    : compositor_(compositor), source_(source) {
  CHECK(compositor_);
  CHECK(source_);
  source_->SetDelegate(this);
}

ExternalBeginFrameAdapter::~ExternalBeginFrameAdapter() {
  source_->SetDelegate(nullptr);
}

void ExternalBeginFrameAdapter::OnBeginFrame(
    base::TimeTicks frame_time,
    base::TimeTicks deadline,
    base::TimeDelta interval,
    base::OnceCallback<void(bool has_damage)> ack_callback) {
  viz::BeginFrameArgs args = args_generator_.GenerateBeginFrameArgs(
      source_id_, frame_time, deadline, interval, interval);

  TRACE_EVENT2("viz", "ExternalBeginFrameAdapter::OnBeginFrame", "frame_time",
               frame_time, "sequence_number", args.frame_id.sequence_number);

  compositor_->IssueExternalBeginFrame(
      args, /*force=*/true,
      base::BindOnce(&ExternalBeginFrameAdapter::OnBeginFrameAck,
                     weak_factory_.GetWeakPtr(), std::move(ack_callback)));
}

mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
ExternalBeginFrameAdapter::CreateExternalBeginFrameControllerClient() {
  TRACE_EVENT0("ui", "ExternalBeginFrameAdapter::CreateClient");
  DVLOG(1) << "ExternalBeginFrameAdapter client bound";
  receivers_.Clear();

  // Disable before enabling to re-initialize after a GPU crash.
  source_->SetNeedsBeginFrame(false);

  mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
      remote;
  receivers_.Add(this, remote.InitWithNewEndpointAndPassReceiver());

  // The first frame will be buffered by Compositor::IssueExternalBeginFrame
  // until the remote is bound in SetExternalBeginFrameController.
  source_->SetNeedsBeginFrame(true);

  return remote;
}

void ExternalBeginFrameAdapter::SetNeedsBeginFrame(bool needs_begin_frames) {
  source_->SetNeedsBeginFrame(needs_begin_frames);
}

void ExternalBeginFrameAdapter::SetPreferredInterval(base::TimeDelta interval) {
  source_->SetPreferredInterval(interval);
}

void ExternalBeginFrameAdapter::NeedsBeginFrameWithId(int64_t display_id,
                                                      bool needs_begin_frames) {
  SetNeedsBeginFrame(needs_begin_frames);
}

void ExternalBeginFrameAdapter::OnBeginFrameAck(
    base::OnceCallback<void(bool)> ack_callback,
    const viz::BeginFrameAck& ack) {
  std::move(ack_callback).Run(ack.has_damage);
}

}  // namespace ui
