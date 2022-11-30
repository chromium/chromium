// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_UNTRUSTED_BUBBLE_WEB_UI_CONTROLLER_H_
#define UI_WEBUI_UNTRUSTED_BUBBLE_WEB_UI_CONTROLLER_H_

#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace content {
class WebUI;
}

namespace ui {

// UntrustedWebUIController is intended for WebUI pages that process untrusted
// content. These WebUIController should never request WebUI bindings, but
// should instead use the WebUI Interface Broker to expose the individual
// interface needed.
class UntrustedBubbleWebUIController : public MojoBubbleWebUIController {
 public:
  explicit UntrustedBubbleWebUIController(content::WebUI* contents,
                                          bool enable_chrome_send = false);
  ~UntrustedBubbleWebUIController() override;
  UntrustedBubbleWebUIController(UntrustedBubbleWebUIController&) = delete;
  UntrustedBubbleWebUIController& operator=(
      const UntrustedBubbleWebUIController&) = delete;
};

}  // namespace ui

#endif  // UI_WEBUI_UNTRUSTED_WEB_UI_CONTROLLER_H_
