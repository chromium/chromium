// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/untrusted_bubble_web_ui_controller.h"

#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"

namespace ui {

UntrustedBubbleWebUIController::UntrustedBubbleWebUIController(
    content::WebUI* web_ui,
    bool enable_chrome_send)
    : MojoBubbleWebUIController(web_ui, enable_chrome_send) {
  // chrome.send() will not work without bindings.
  CHECK(!enable_chrome_send);
  // UntrustedWebUIController should never enable WebUI bindings that expose
  // all of the browser interfaces.
  web_ui->SetBindings(content::BINDINGS_POLICY_NONE);
}

UntrustedBubbleWebUIController::~UntrustedBubbleWebUIController() = default;

}  // namespace ui