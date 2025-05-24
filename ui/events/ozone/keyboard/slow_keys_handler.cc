// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/keyboard/slow_keys_handler.h"

#include "base/containers/map_util.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"

namespace ui {

namespace {

// Default delay duration.
constexpr base::TimeDelta kDefaultDelay = base::Milliseconds(500);
// Max delay duration.
constexpr base::TimeDelta kMaxDelay = base::Seconds(10);

}  // namespace

SlowKeysHandler::SlowKeysHandler() : delay_(kDefaultDelay) {}

SlowKeysHandler::~SlowKeysHandler() = default;

bool SlowKeysHandler::IsEnabled() const {
  return enabled_;
}

void SlowKeysHandler::SetEnabled(bool enabled) {
  if (!enabled) {
    Clear();
  }
  enabled_ = enabled;
}

base::TimeDelta SlowKeysHandler::GetDelay() const {
  return delay_;
}

void SlowKeysHandler::SetDelay(base::TimeDelta delay) {
  if (delay != delay_) {
    Clear();
  }
  if (delay.is_positive() && delay < kMaxDelay) {
    delay_ = delay;
  }
}

void SlowKeysHandler::Clear() {
  delayed_keys_map_.clear();
}

bool SlowKeysHandler::UpdateKeyStateAndShouldDispatch(
    unsigned int key,
    bool down,
    base::TimeTicks timestamp,
    int device_id,
    OnKeyChangeCallback callback) {
  if (!enabled_) {
    return true;
  }

  DelayMapKey delay_key{
      .device_id = device_id,
      .key = key,
  };

  auto* old_timer = base::FindPtrOrNull(delayed_keys_map_, delay_key);
  bool had_timer = old_timer;
  bool pending = old_timer && old_timer->IsRunning();

  if (!down) {
    delayed_keys_map_.erase(delay_key);
  }
  // Don't use `old_timer` after this, since it may have been invalidated.

  if (!had_timer && down) {
    auto timer = std::make_unique<base::OneShotTimer>();
    timer->Start(FROM_HERE, delay_,
                 base::BindOnce(&SlowKeysHandler::OnDelayReached,
                                weak_ptr_factory_.GetWeakPtr(),
                                /*input_timestamp=*/timestamp,
                                /*system_timestamp=*/EventTimeForNow(),
                                std::move(callback)));
    delayed_keys_map_[delay_key] = std::move(timer);
    return false;
  } else if (pending && !processing_delayed_key_) {
    return false;
  }
  return true;
}

void SlowKeysHandler::OnDelayReached(base::TimeTicks input_timestamp,
                                     base::TimeTicks system_timestamp,
                                     OnKeyChangeCallback callback) {
  if (!callback) {
    return;
  }

  auto delta = EventTimeForNow() - system_timestamp;
  auto new_timestamp = input_timestamp + delta;

  // The OnKeyChangeCallback could call `UpdateKeyStateAndShouldDispatch` again,
  // so `processing_delayed_key_` signals the recursive call to prevent infinite
  // loop. This relies on the fact that this handler is used in a
  // single-threaded context that processes one key at a time. If that
  // assumption is ever broken, this logic will need to be updated.
  processing_delayed_key_ = true;
  std::move(callback).Run(new_timestamp);
  processing_delayed_key_ = false;
}

}  // namespace ui
