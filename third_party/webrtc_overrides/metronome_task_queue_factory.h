// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_METRONOME_TASK_QUEUE_FACTORY_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_METRONOME_TASK_QUEUE_FACTORY_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace blink {

// Whether WebRTC should use a metronome-backed task queue. Default: disabled.
RTC_EXPORT extern const base::Feature kWebRtcMetronomeTaskQueue;

// Feature params for the metronome task queue. Example uses:
//
// --enable-features=WebRtcMetronomeTaskQueue:tick/10ms/exclude_pacer/false
//
// Maximum usage of metronome, including decoding queues:
// --enable-features=WebRtcMetronomeTaskQueue:exclude_decoders/false/exclude_pacer/false/exclude_misc/false
//
// Metronoome decoding queues, but nothing else:
// --enable-features=WebRtcMetronomeTaskQueue:exclude_decoders/false/exclude_pacer/true/exclude_misc/true

// Specify the desired metronome tick interval with "tick". Default: 64 Hz.
RTC_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kWebRtcMetronomeTaskQueueTick;
// Specify if the pacer should be excluded with "exclude_pacer". Default: true.
RTC_EXPORT extern const base::FeatureParam<bool>
    kWebRtcMetronomeTaskQueueExcludePacer;
// Specify if decoding queues should be excluded with "exclude_decoders".
// Default: true.
RTC_EXPORT extern const base::FeatureParam<bool>
    kWebRtcMetronomeTaskQueueExcludeDecoders;
// Specify if other tasks (tasks not specified by above arguments) should be
// excluded with "exclude_misc". Default: false.
RTC_EXPORT extern const base::FeatureParam<bool>
    kWebRtcMetronomeTaskQueueExcludeMisc;

}  // namespace blink

// Creates a factory for webrtc::TaskQueueBase that is backed by a
// blink::MetronomeSource. Tested by
// /third_party/blink/renderer/platform/peerconnection/metronome_task_queue_factory_test.cc
RTC_EXPORT std::unique_ptr<webrtc::TaskQueueFactory>
CreateWebRtcMetronomeTaskQueueFactory(
    scoped_refptr<blink::MetronomeSource> metronome_source);

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_METRONOME_TASK_QUEUE_FACTORY_H_
