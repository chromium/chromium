// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

#include "base/types/pass_key.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/webui/examples/browser/browser_main_parts.h"
#import "ui/webui/examples/browser/ui/cocoa/content_ns_window.h"

namespace webui_examples {

class BrowserMainPartsMac : public BrowserMainParts {
 public:
  using PassKey = base::PassKey<BrowserMainParts>;
  explicit BrowserMainPartsMac(PassKey) {}
  BrowserMainPartsMac(const BrowserMainPartsMac&) = delete;
  const BrowserMainPartsMac& operator=(const BrowserMainPartsMac&) = delete;
  ~BrowserMainPartsMac() override = default;

 private:
  void InitializeUiToolkit() override {}

  void ShutdownUiToolkit() override {}

  void CreateAndShowWindowForWebContents(
      std::unique_ptr<content::WebContents> web_contents,
      const std::u16string& title) override {
    std::unique_ptr<ContentNSWindow> content_window =
        std::make_unique<ContentNSWindow>(std::move(web_contents));
    ContentNSWindow* content_window_ptr = content_window.get();
    content_window_ptr->SetTitle(title);
    content_window_ptr->Show();
    content_window_ptr->SetCloseCallback(
        base::BindOnce(&BrowserMainPartsMac::OnWindowClosed,
                       weak_factory_.GetWeakPtr(), std::move(content_window)));
  }

  void OnWindowClosed(std::unique_ptr<ContentNSWindow> content_ns_window) {
    // We are dispatching a callback that originates from the content_ns_window.
    // Deleting soon instead of now eliminates the chance of a crash in case the
    // content_window or associated objects have more work to do after this
    // callback.
    content::GetUIThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, std::move(content_ns_window));
    BrowserMainParts::OnWindowClosed();
  }

  base::WeakPtrFactory<BrowserMainPartsMac> weak_factory_{this};
};

// static
std::unique_ptr<BrowserMainParts> BrowserMainParts::Create() {
  return std::make_unique<BrowserMainPartsMac>(BrowserMainPartsMac::PassKey());
}

}  // namespace webui_examples
