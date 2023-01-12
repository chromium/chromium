// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/user_active_simulator.h"
#include "base/functional/bind.h"

namespace power_sampler {

UserActiveSimulator::UserActiveSimulator() = default;

UserActiveSimulator::~UserActiveSimulator() = default;

void UserActiveSimulator::Start() {
  // macOS considers the user idle after 5 minutes of inactivity. This simulates
  // user activity every 4 minutes 55 seconds so that the user is always
  // considered active.
  timer_.Start(FROM_HERE, base::Minutes(4) + base::Seconds(55),
               base::BindRepeating(&UserActiveSimulator::OnTimer,
                                   base::Unretained(this)));
}

void UserActiveSimulator::OnTimer() {
  IOPMAssertionDeclareUserActivity(CFSTR("User Active Simulator"),
                                   kIOPMUserActiveLocal, &assertion_id_);
}

}  // namespace power_sampler
