// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_TYPING_SESSION_MANAGER_H_
#define UI_BASE_IME_ASH_TYPING_SESSION_MANAGER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"

namespace ash {

class TypingSessionManager {
 public:
  explicit TypingSessionManager(base::Clock* clock);

  TypingSessionManager(const TypingSessionManager& typing_session_manager);

  ~TypingSessionManager();

  // To be called whenever user activity is detected to keep the
  // session going.
  void Heartbeat();

  // Record that |character_count| characters have been committed
  // This also triggers a Heartbeat.
  void CommitCharacters(uint64_t character_count);

  // End a typing session, record the metrics and
  void EndAndRecordSession();

 private:
  uint64_t characters_committed_this_session_count_;

  base::Time session_start_time_;

  base::Time last_user_action_time_;

  raw_ptr<base::Clock> clock_;

  bool is_active_;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_TYPING_SESSION_MANAGER_H_
