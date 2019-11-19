// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_client_main_parts_aura.h"

#include "ui/wm/core/wm_state.h"

namespace ui {

ViewsContentClientMainPartsAura::ViewsContentClientMainPartsAura(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainParts(content_params, views_content_client) {
}

ViewsContentClientMainPartsAura::~ViewsContentClientMainPartsAura() {
}

void ViewsContentClientMainPartsAura::ToolkitInitialized() {
  ViewsContentClientMainParts::ToolkitInitialized();

  wm_state_ = std::make_unique<::wm::WMState>();
}

void ViewsContentClientMainPartsAura::PostMainMessageLoopRun() {
  ViewsContentClientMainParts::PostMainMessageLoopRun();
}

}  // namespace ui
