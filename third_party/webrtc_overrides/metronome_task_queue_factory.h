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

namespace blink {

// Whether WebRTC should use a metronome-backed task queue. Default: disabled.
RTC_EXPORT extern const base::Feature kWebRtcMetronomeTaskQueue;

}  // namespace blink

// Creates a factory for webrtc::TaskQueueBase that is backed by a
// blink::MetronomeSource. Tested by
// /third_party/blink/renderer/platform/peerconnection/metronome_task_queue_factory_test.cc
RTC_EXPORT std::unique_ptr<webrtc::TaskQueueFactory>
CreateWebRtcMetronomeTaskQueueFactory();

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_METRONOME_TASK_QUEUE_FACTORY_H_
