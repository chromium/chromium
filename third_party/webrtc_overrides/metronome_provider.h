// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_METRONOME_PROVIDER_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_METRONOME_PROVIDER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace blink {

// The listener of a MetronomeProvider is notified when the metronome starts or
// stops being allowed to be used.
class RTC_EXPORT MetronomeProviderListener {
 public:
  virtual ~MetronomeProviderListener();

  virtual void OnStartUsingMetronome(
      scoped_refptr<MetronomeSource> metronome) = 0;
  virtual void OnStopUsingMetronome() = 0;
};

// Forwards call when a metronome starts or stops being usable to its listeners.
class RTC_EXPORT MetronomeProvider
    : public base::RefCountedThreadSafe<MetronomeProvider> {
 public:
  ~MetronomeProvider();

  // Manipulate the list of listeners.
  void AddListener(MetronomeProviderListener* listener);
  void RemoveListener(MetronomeProviderListener* listener);

  // Forwards the call to all listeners.
  void OnStartUsingMetronome(scoped_refptr<MetronomeSource> metronome);
  void OnStopUsingMetronome();

 private:
  base::Lock lock_;
  scoped_refptr<MetronomeSource> metronome_ GUARDED_BY(lock_);
  std::vector<MetronomeProviderListener*> listeners_ GUARDED_BY(lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_METRONOME_PROVIDER_H_
