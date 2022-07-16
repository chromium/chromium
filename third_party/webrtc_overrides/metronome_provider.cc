// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_provider.h"

#include <algorithm>

namespace blink {

MetronomeProviderListener::~MetronomeProviderListener() = default;

MetronomeProvider::~MetronomeProvider() {
  DCHECK(listeners_.empty());
}

void MetronomeProvider::AddListener(MetronomeProviderListener* listener) {
  base::AutoLock auto_lock(lock_);
  listeners_.push_back(listener);
  if (metronome_) {
    // We already have a metronome, informm immediately.
    listener->OnStartUsingMetronome(metronome_);
  }
}

void MetronomeProvider::RemoveListener(MetronomeProviderListener* listener) {
  base::AutoLock auto_lock(lock_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it == listeners_.end()) {
    return;
  }
  listeners_.erase(it);
}

void MetronomeProvider::OnStartUsingMetronome(
    scoped_refptr<MetronomeSource> metronome) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!metronome_);
  DCHECK(metronome);
  metronome_ = metronome;
  for (auto* listener : listeners_) {
    listener->OnStartUsingMetronome(metronome_);
  }
}

void MetronomeProvider::OnStopUsingMetronome() {
  base::AutoLock auto_lock(lock_);
  metronome_ = nullptr;
  for (auto* listener : listeners_) {
    listener->OnStopUsingMetronome();
  }
}

}  // namespace blink
