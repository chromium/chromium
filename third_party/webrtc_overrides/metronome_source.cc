// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_source.h"
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/webrtc/api/metronome/metronome.h"

namespace blink {

constexpr base::TimeDelta kMetronomeTick = base::Hertz(64);

class WebRtcMetronomeAdapter : public webrtc::Metronome {
 public:
  explicit WebRtcMetronomeAdapter(base::WeakPtr<MetronomeSource> source)
      : source_(std::move(source)) {}

  // webrtc::Metronome implementation.
  webrtc::TimeDelta TickPeriod() const override {
    return webrtc::TimeDelta::Micros(MetronomeSource::Tick().InMicroseconds());
  }

  void RequestCallOnNextTick(absl::AnyInvocable<void() &&> callback) override {
    if (source_)
      source_->RequestCallOnNextTick(std::move(callback));
  }

 private:
  const base::WeakPtr<MetronomeSource> source_;
};

// static
base::TimeTicks MetronomeSource::Phase() {
  return base::TimeTicks();
}

// static
base::TimeDelta MetronomeSource::Tick() {
  return kMetronomeTick;
}

// static
base::TimeTicks MetronomeSource::TimeSnappedToNextTick(base::TimeTicks time) {
  return time.SnappedToNextTick(MetronomeSource::Phase(),
                                MetronomeSource::Tick());
}

MetronomeSource::MetronomeSource(
    const scoped_refptr<base::SequencedTaskRunner>& metronome_task_runner)
    : metronome_task_runner_(metronome_task_runner) {
  DETACH_FROM_SEQUENCE(metronome_sequence_checker_);
}

MetronomeSource::~MetronomeSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(metronome_sequence_checker_);
}

void MetronomeSource::RequestCallOnNextTick(
    absl::AnyInvocable<void() &&> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(metronome_sequence_checker_);
  callbacks_.push_back(std::move(callback));
  if (callbacks_.size() == 1)
    Reschedule();
}

void MetronomeSource::OnMetronomeTick() {
  TRACE_EVENT_INSTANT0("webrtc", "MetronomeSource::OnMetronomeTick",
                       TRACE_EVENT_SCOPE_PROCESS);
  TRACE_EVENT0("webrtc", "MetronomeSource::OnMetronomeTick");
  DCHECK_CALLED_ON_VALID_SEQUENCE(metronome_sequence_checker_);

  std::vector<absl::AnyInvocable<void() &&>> callbacks;
  callbacks.swap(callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback)();
  }
}

void MetronomeSource::Reschedule() {
  metronome_task_runner_->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&MetronomeSource::OnMetronomeTick,
                     weak_factory_.GetWeakPtr()),
      TimeSnappedToNextTick(base::TimeTicks::Now()),
      base::subtle::DelayPolicy::kPrecise);
}

std::unique_ptr<webrtc::Metronome> MetronomeSource::CreateWebRtcMetronome() {
  return std::make_unique<WebRtcMetronomeAdapter>(weak_factory_.GetWeakPtr());
}

}  // namespace blink
