// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/examples/browser/ui/web/browser.mojom.h"

namespace webui_examples {

class Browser;

class BrowserPageHandler
    : public content::DocumentService<webui_examples::mojom::PageHandler> {
 public:
  BrowserPageHandler(const BrowserPageHandler&) = delete;
  BrowserPageHandler& operator=(const BrowserPageHandler&) = delete;
  ~BrowserPageHandler() override;

  static BrowserPageHandler* Create(
      Browser* webui_controller,
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver);

  // webui_examples::mojom::PageHandler
  void Navigate(int32_t guest_instance_id, const GURL& src) override;
  void GoBack(int32_t guest_instance_id) override;
  void GoForward(int32_t guest_instance_id) override;

  // The WebUI controller goes away before the Document. Clear the raw_ptr
  // to the controller to avoid it becoming dangling.
  void WebUIControllerDestroyed();

 private:
  BrowserPageHandler(
      Browser* webui_controller,
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver);

  Browser* webui_controller() { return webui_controller_; }

  raw_ptr<Browser> webui_controller_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_
