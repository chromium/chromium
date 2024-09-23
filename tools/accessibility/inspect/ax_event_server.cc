// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_event_server.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

namespace tools {

AXEventServer::AXEventServer(base::ProcessId pid,
                             const ui::AXTreeSelector& selector,
                             const ui::AXInspectScenario& scenario)
    : recorder_(content::AXInspectFactory::CreateRecorder(
          content::AXInspectFactory::DefaultPlatformRecorderType(),
          nullptr,
          pid,
          selector)) {
  recorder_->SetPropertyFilters(scenario.property_filters);
  recorder_->ListenToEvents(
      base::BindRepeating(&AXEventServer::OnEvent, base::Unretained(this)));
}

AXEventServer::~AXEventServer() = default;

void AXEventServer::OnEvent(const std::string& event) const {
  LOG(INFO) << "[" << base::Time::NowFromSystemTime() << "] " << event;
}

}  // namespace tools
