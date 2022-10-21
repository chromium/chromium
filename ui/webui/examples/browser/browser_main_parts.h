// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_
#define UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "content/public/browser/browser_main_parts.h"

class GURL;

namespace content {
class BrowserContext;
class DevToolsManagerDelegate;
class WebContents;
class WebContentsViewDelegate;
}  // namespace content

namespace webui_examples {

class AuraContext;
class BrowserContext;
class ContentWindow;
class WebUIControllerFactory;

class BrowserMainParts : public content::BrowserMainParts {
 public:
  BrowserMainParts();
  BrowserMainParts(const BrowserMainParts&) = delete;
  BrowserMainParts& operator=(const BrowserMainParts&) = delete;
  ~BrowserMainParts() override;

  std::unique_ptr<content::WebContentsViewDelegate>
  CreateWebContentsViewDelegate(content::WebContents* web_contents);
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate();

 private:
  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

  // content::WebContents is associated and bound to the lifetime of the window.
  content::WebContents* CreateAndShowContentWindow(GURL url,
                                                   const std::u16string& title);
  void OnWindowClosed(std::unique_ptr<ContentWindow> content_window);
  void QuitMessageLoop();

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<WebUIControllerFactory> web_ui_controller_factory_;
  std::unique_ptr<content::BrowserContext> browser_context_;

  std::unique_ptr<AuraContext> aura_context_;
  int content_windows_outstanding_ = 0;

  base::RepeatingClosure quit_run_loop_;

  base::WeakPtrFactory<BrowserMainParts> weak_factory_{this};
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_
