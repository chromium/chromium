// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_event_server.h"

#include "base/bind.h"

namespace tools {

AXEventServer::AXEventServer(
    base::ProcessId pid,
    const content::AccessibilityTreeFormatter::TreeSelector& selector)
    : recorder_(
          content::AccessibilityEventRecorder::Create(nullptr, pid, selector)) {
  recorder_->ListenToEvents(
      base::BindRepeating(&AXEventServer::OnEvent, base::Unretained(this)));
}

AXEventServer::~AXEventServer() = default;

void AXEventServer::OnEvent(const std::string& event) const {
  printf("%s\n", event.c_str());
}

}  // namespace tools
