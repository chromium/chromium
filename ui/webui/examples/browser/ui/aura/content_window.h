// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_CONTENT_WINDOW_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_CONTENT_WINDOW_H_

#include "base/functional/callback.h"
#include "ui/webui/examples/browser/ui/aura/aura_context.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace wm {
class CompoundEventFilter;
}  // namespace wm

namespace webui_examples {

// Represents a single window that hosts one WebContents stretched to the
// window's size.
class ContentWindow {
 public:
  ContentWindow(AuraContext* aura_context,
                std::unique_ptr<content::WebContents> web_contents);
  ContentWindow(const ContentWindow&) = delete;
  ContentWindow& operator=(const ContentWindow&) = delete;
  ~ContentWindow();

  void SetTitle(const std::u16string& title);

  void Show();
  void SetCloseCallback(base::OnceClosure on_close);

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  std::unique_ptr<AuraContext::ContextualizedWindowTreeHost> host_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<wm::CompoundEventFilter> root_window_event_filter_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_CONTENT_WINDOW_H_
