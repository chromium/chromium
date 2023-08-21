// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/examples/browser/ui/web/browser.mojom.h"

namespace webui_examples {

class BrowserPageHandler : public webui_examples::mojom::PageHandler {
 public:
  explicit BrowserPageHandler(
      mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver);
  BrowserPageHandler(const BrowserPageHandler&) = delete;
  BrowserPageHandler& operator=(const BrowserPageHandler&) = delete;
  ~BrowserPageHandler() override;

  // webui_examples::mojom::PageHandler
  void Navigate(int32_t view_instance_id, const GURL& src) override;
  void GoBack(int32_t view_instance_id) override;
  void GoForward(int32_t view_instance_id) override;

 private:
  mojo::Receiver<webui_examples::mojom::PageHandler> receiver_;
};

}  // namespace webui_examples

#endif  //  UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_
