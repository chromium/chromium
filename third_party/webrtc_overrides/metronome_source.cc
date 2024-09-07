// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_source.h"
#include <memory>
#include <utility>

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
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace blink {
class WebRtcMetronomeAdapter : public webrtc::Metronome {
 public:
  explicit WebRtcMetronomeAdapter(base::WeakPtr<MetronomeSource> source)
      : source_(std::move(source)) {}

  // webrtc::Metronome implementation.
  webrtc::TimeDelta TickPeriod() const override {
    const base::TimeDelta period = source_
                                       ? source_->TickPeriod()
                                       : TimerBasedTickProvider::kDefaultPeriod;
    return webrtc::TimeDelta::Micros(period.InMicroseconds());
  }

  void RequestCallOnNextTick(absl::AnyInvocable<void() &&> callback) override {
    if (source_)
      source_->RequestCallOnNextTick(std::move(callback));
  }

 private:
  const base::WeakPtr<MetronomeSource> source_;
};

MetronomeSource::MetronomeSource(scoped_refptr<TickProvider> tick_provider)
    : tick_provider_(std::move(tick_provider)) {
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(metronome_sequence_checker_);
  tick_provider_->RequestCallOnNextTick(base::BindOnce(
      &MetronomeSource::OnMetronomeTick, weak_factory_.GetWeakPtr()));
}

base::TimeDelta MetronomeSource::TickPeriod() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(metronome_sequence_checker_);
  return tick_provider_->TickPeriod();
}

std::unique_ptr<webrtc::Metronome> MetronomeSource::CreateWebRtcMetronome() {
  return std::make_unique<WebRtcMetronomeAdapter>(weak_factory_.GetWeakPtr());
}

}  // namespace blink
