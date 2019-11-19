// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/devtools_manager_delegate_android.h"

namespace weblayer {

DevToolsManagerDelegateAndroid::DevToolsManagerDelegateAndroid() = default;

DevToolsManagerDelegateAndroid::~DevToolsManagerDelegateAndroid() = default;

std::string DevToolsManagerDelegateAndroid::GetDiscoveryPageHTML() {
  const char html[] =
      "<html>"
      "<head><title>WebLayer remote debugging</title></head>"
      "<body>Please use <a href=\'chrome://inspect\'>chrome://inspect</a>"
      "</body>"
      "</html>";
  return html;
}

bool DevToolsManagerDelegateAndroid::IsBrowserTargetDiscoverable() {
  return true;
}

}  // namespace weblayer
