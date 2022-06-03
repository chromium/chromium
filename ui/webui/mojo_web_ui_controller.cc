// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/mojo_web_ui_controller.h"

#include "content/public/common/bindings_policy.h"

namespace ui {

MojoWebUIController::MojoWebUIController(content::WebUI* contents,
                                         bool enable_chrome_send)
    : content::WebUIController(contents) {
  int bindings = content::BINDINGS_POLICY_MOJO_WEB_UI;
  if (enable_chrome_send)
    bindings |= content::BINDINGS_POLICY_WEB_UI;
  contents->SetBindings(bindings);
}
MojoWebUIController::~MojoWebUIController() = default;

}  // namespace ui
