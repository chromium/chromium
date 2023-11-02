// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_UNTRUSTED_WEB_UI_CONTROLLER_H_
#define UI_WEBUI_UNTRUSTED_WEB_UI_CONTROLLER_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

namespace ui {

// UntrustedWebUIController is intended for WebUI pages that process untrusted
// content. These WebUIController should never request WebUI bindings.
class UntrustedWebUIController : public content::WebUIController {
 public:
  explicit UntrustedWebUIController(content::WebUI* contents);
  ~UntrustedWebUIController() override;
  UntrustedWebUIController(UntrustedWebUIController&) = delete;
  UntrustedWebUIController& operator=(const UntrustedWebUIController&) = delete;
};

}  // namespace ui

#endif  // UI_WEBUI_UNTRUSTED_WEB_UI_CONTROLLER_H_
