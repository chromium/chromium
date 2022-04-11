// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_client_main_parts_aura.h"

#include <utility>

#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/wm/core/wm_state.h"
#endif

namespace ui {

ViewsContentClientMainPartsAura::ViewsContentClientMainPartsAura(
    content::MainFunctionParams content_params,
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainParts(std::move(content_params),
                                  views_content_client) {}

ViewsContentClientMainPartsAura::~ViewsContentClientMainPartsAura() {
}

void ViewsContentClientMainPartsAura::ToolkitInitialized() {
  ViewsContentClientMainParts::ToolkitInitialized();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  wm_state_ = std::make_unique<::wm::WMState>();
#endif
}

void ViewsContentClientMainPartsAura::PostMainMessageLoopRun() {
  ViewsContentClientMainParts::PostMainMessageLoopRun();
}

}  // namespace ui
