// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/webui_config.h"

namespace ui {

WebUIConfig::WebUIConfig(base::StringPiece scheme, base::StringPiece host)
    : scheme_(scheme), host_(host) {}

WebUIConfig::~WebUIConfig() = default;

bool WebUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return true;
}

}  // namespace ui
