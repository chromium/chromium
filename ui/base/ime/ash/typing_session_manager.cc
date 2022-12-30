// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/typing_session_manager.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/clock.h"

namespace ash {

const uint64_t INACTIVITY_TIMEOUT_FOR_SESSION_IN_MS = 3000;
const uint64_t MIN_CHARACTERS_COMMITTED = 10;
const base::TimeDelta MIN_SESSION_DURATION_IN_MS = base::Seconds(1);

// TODO: We cannot assume that the time will always increase, it can decrease if
// system clock is explicitly set. Invalidate sessions where system clock
// decreases.
TypingSessionManager::TypingSessionManager(base::Clock* clock)
    : characters_committed_this_session_count_(0),
      clock_(clock ? clock : base::DefaultClock::GetInstance()),
      is_active_(false) {
  session_start_time_ = clock_->Now();
  last_user_action_time_ = clock_->Now();
}

TypingSessionManager::TypingSessionManager(
    const TypingSessionManager& typing_session_manager) {}

TypingSessionManager::~TypingSessionManager() {
  EndAndRecordSession();
}

void TypingSessionManager::Heartbeat() {
  base::Time current_time = clock_->Now();

  // If too much time has passed, then end the previous session and
  // start a new one.
  if (is_active_) {
    if (last_user_action_time_ +
            base::Milliseconds(INACTIVITY_TIMEOUT_FOR_SESSION_IN_MS) <
        current_time) {
      EndAndRecordSession();
    }
  }

  last_user_action_time_ = current_time;
  // If no session is active, create a new one here.
  if (!is_active_) {
    is_active_ = true;
    session_start_time_ = current_time;
    characters_committed_this_session_count_ = 0;
  }
}

// Note: only count characters as committed if the VK is enabled.
void TypingSessionManager::CommitCharacters(uint64_t character_count) {
  Heartbeat();
  characters_committed_this_session_count_ += character_count;
}

void TypingSessionManager::EndAndRecordSession() {
  // Only record for sessions where at least 10 characters were entered
  base::TimeDelta time_taken = last_user_action_time_ - session_start_time_;
  if (characters_committed_this_session_count_ >= MIN_CHARACTERS_COMMITTED &&
      time_taken >= MIN_SESSION_DURATION_IN_MS) {
    base::UmaHistogramMediumTimes("InputMethod.SessionDuration", time_taken);
    base::UmaHistogramCounts100000("InputMethod.CharactersPerSession",
                                   characters_committed_this_session_count_);
  }

  // Reset the typing session
  characters_committed_this_session_count_ = 0;
  is_active_ = false;
  session_start_time_ = clock_->Now();
  last_user_action_time_ = clock_->Now();
}

}  // namespace ash
