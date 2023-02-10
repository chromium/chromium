// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_main_parts.h"

#include "wolvic/wolvic_browser_context.h"

namespace content {

WolvicMainParts::WolvicMainParts() {}

WolvicMainParts::~WolvicMainParts() {}

int WolvicMainParts::PreMainMessageLoopRun() {
  browser_context_ = std::make_unique<WolvicBrowserContext>();
  return 0;
}

void WolvicMainParts::set_browser_context(WolvicBrowserContext* context) {
  browser_context_.reset(context);
}

}  // namespace content
