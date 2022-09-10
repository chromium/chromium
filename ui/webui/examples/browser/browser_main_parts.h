// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_
#define UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "content/public/browser/browser_main_parts.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace webui_examples {

class BrowserContext;

class BrowserMainParts : public content::BrowserMainParts {
 public:
  BrowserMainParts();
  BrowserMainParts(const BrowserMainParts&) = delete;
  BrowserMainParts& operator=(const BrowserMainParts&) = delete;
  ~BrowserMainParts() override;

 private:
  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<content::BrowserContext> browser_context_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_
