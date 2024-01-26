// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_
#define UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"

#if BUILDFLAG(IS_MAC)
#include "ui/display/screen.h"
#endif  // BUILDFLAG(IS_MAC)

class GURL;

namespace content {
class BrowserContext;
class DevToolsManagerDelegate;
class WebContents;
class WebContentsViewDelegate;
}  // namespace content

namespace webui_examples {

class WebUIControllerFactory;

class BrowserMainParts : public content::BrowserMainParts {
 public:
  static std::unique_ptr<BrowserMainParts> Create();

  BrowserMainParts(const BrowserMainParts&) = delete;
  BrowserMainParts& operator=(const BrowserMainParts&) = delete;
  ~BrowserMainParts() override;

  std::unique_ptr<content::WebContentsViewDelegate>
  CreateWebContentsViewDelegate(content::WebContents* web_contents);
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate();

 protected:
  BrowserMainParts();
  virtual void InitializeUiToolkit();
  virtual void ShutdownUiToolkit();
  virtual void CreateAndShowWindowForWebContents(
      std::unique_ptr<content::WebContents> web_contents,
      const std::u16string& title) = 0;

  content::BrowserContext* browser_context() { return browser_context_.get(); }

  void OnWindowClosed();

 private:
  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

  content::WebContents* CreateAndShowWindow(GURL url,
                                            const std::u16string& title);

  void QuitMessageLoop();

  base::ScopedTempDir temp_dir_;
#if BUILDFLAG(IS_MAC)
  display::ScopedNativeScreen native_screen_;
#endif
  std::unique_ptr<WebUIControllerFactory> web_ui_controller_factory_;
  std::unique_ptr<content::BrowserContext> browser_context_;

  int content_windows_outstanding_ = 0;

  base::RepeatingClosure quit_run_loop_;

  base::WeakPtrFactory<BrowserMainParts> weak_factory_{this};
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_BROWSER_MAIN_PARTS_H_
