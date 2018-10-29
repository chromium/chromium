// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_event_server.h"

#include <iostream>
#include <string>

#include "base/bind.h"

namespace tools {

AXEventServer::AXEventServer(base::ProcessId pid,
                             const base::StringPiece& pattern)
    : recorder_(
          content::AccessibilityEventRecorder::Create(nullptr, pid, pattern)) {
  recorder_->ListenToEvents(
      base::BindRepeating(&AXEventServer::OnEvent, base::Unretained(this)));

  std::stringstream output;
  output << "Events for process id: " << pid;
  printf("%s", output.str().c_str());
}

AXEventServer::~AXEventServer() = default;

void AXEventServer::OnEvent(const std::string& event) const {
  printf("%s\n", event.c_str());
}

}  // namespace tools
