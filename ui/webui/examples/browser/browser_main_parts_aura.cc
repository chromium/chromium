// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/pass_key.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/webui/examples/browser/browser_main_parts.h"
#include "ui/webui/examples/browser/ui/aura/aura_context.h"
#include "ui/webui/examples/browser/ui/aura/content_window.h"

namespace webui_examples {

class BrowserMainPartsAura : public BrowserMainParts {
 public:
  using PassKey = base::PassKey<BrowserMainParts>;
  explicit BrowserMainPartsAura(PassKey) {}
  BrowserMainPartsAura(const BrowserMainPartsAura&) = delete;
  const BrowserMainPartsAura& operator=(const BrowserMainPartsAura&) = delete;
  ~BrowserMainPartsAura() override = default;

 private:
  void InitializeUiToolkit() override {
    aura_context_ = std::make_unique<AuraContext>();
  }

  void ShutdownUiToolkit() override { aura_context_.reset(); }

  void CreateAndShowWindowForWebContents(
      std::unique_ptr<content::WebContents> web_contents,
      const std::u16string& title) override {
    auto content_window = std::make_unique<ContentWindow>(
        aura_context_.get(), std::move(web_contents));
    ContentWindow* content_window_ptr = content_window.get();
    content_window_ptr->SetTitle(title);
    content_window_ptr->Show();
    content_window_ptr->SetCloseCallback(
        base::BindOnce(&BrowserMainPartsAura::OnWindowClosed,
                       weak_factory_.GetWeakPtr(), std::move(content_window)));
  }

  void OnWindowClosed(std::unique_ptr<ContentWindow> content_window) {
    // We are dispatching a callback that originates from the content_window.
    // Deleting soon instead of now eliminates the chance of a crash in case the
    // content_window or associated objects have more work to do after this
    // callback.
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   std::move(content_window));
    BrowserMainParts::OnWindowClosed();
  }

  std::unique_ptr<AuraContext> aura_context_;
  base::WeakPtrFactory<BrowserMainPartsAura> weak_factory_{this};
};

// static
std::unique_ptr<BrowserMainParts> BrowserMainParts::Create() {
  return std::make_unique<BrowserMainPartsAura>(
      BrowserMainPartsAura::PassKey());
}

}  // namespace webui_examples
