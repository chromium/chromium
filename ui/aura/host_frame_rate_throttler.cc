// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/host_frame_rate_throttler.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace aura {

#if BUILDFLAG(IS_WIN)
constexpr uint8_t kDefaultThrottleFps = 1;
#else
constexpr uint8_t kDefaultThrottleFps = 20;
#endif

HostFrameRateThrottler& HostFrameRateThrottler::GetInstance() {
  static base::NoDestructor<HostFrameRateThrottler> instance;
  return *instance;
}

HostFrameRateThrottler::HostFrameRateThrottler() = default;

HostFrameRateThrottler::~HostFrameRateThrottler() = default;

void HostFrameRateThrottler::AddHost(WindowTreeHost* host) {
  if (base::Contains(hosts_, host))
    return;
  hosts_.insert(host);
  UpdateHostFrameSinkManager();
}

void HostFrameRateThrottler::RemoveHost(WindowTreeHost* host) {
  if (!base::Contains(hosts_, host))
    return;
  hosts_.erase(host);
  UpdateHostFrameSinkManager();
}

void HostFrameRateThrottler::UpdateHostFrameSinkManager() {
  std::vector<viz::FrameSinkId> ids;
  ids.reserve(hosts_.size());
  for (WindowTreeHost* host : hosts_)
    ids.push_back(host->compositor()->frame_sink_id());
  // `ContextFactory` may be null on shutdown.
  if (Env::GetInstance()->context_factory()) {
    Env::GetInstance()->context_factory()->GetHostFrameSinkManager()->Throttle(
        ids, base::Hertz(kDefaultThrottleFps));
  }
}

}  // namespace aura
