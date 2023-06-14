// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ui/webui/examples/browser/browser_main_parts.h"

namespace webui_examples {

// static
std::unique_ptr<BrowserMainParts> BrowserMainParts::Create() {
  NOTREACHED();
  return nullptr;
}

}  // namespace webui_examples
