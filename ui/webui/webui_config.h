// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_WEBUI_CONFIG_H_
#define UI_WEBUI_WEBUI_CONFIG_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"

namespace content {
class BrowserContext;
class WebUIController;
class WebUI;
}  // namespace content

namespace ui {

// Class that stores properties for a WebUI.
class WebUIConfig {
 public:
  explicit WebUIConfig(base::StringPiece scheme, base::StringPiece host);
  virtual ~WebUIConfig();
  WebUIConfig(const WebUIConfig&) = delete;
  WebUIConfig& operator=(const WebUIConfig&) = delete;

  // Scheme for the WebUI.
  const std::string& scheme() const { return scheme_; }

  // Host the WebUI serves.
  const std::string& host() const { return host_; }

  // Returns whether the WebUI is enabled e.g. the necessary feature flags are
  // on/off, the WebUI is enabled in incognito, etc. Defaults to true.
  virtual bool IsWebUIEnabled(content::BrowserContext* browser_context);

  // Returns a WebUIController for the WebUI.
  virtual std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) = 0;

 private:
  const std::string scheme_;
  const std::string host_;
};

}  // namespace ui

#endif  // UI_WEBUI_WEBUI_CONFIG_H_
