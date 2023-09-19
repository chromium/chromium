// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_

#include "base/values.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/examples/browser/ui/web/browser.mojom.h"

namespace webui_examples {

class BrowserPageHandler
    : public content::DocumentService<webui_examples::mojom::PageHandler> {
 public:
  BrowserPageHandler(const BrowserPageHandler&) = delete;
  BrowserPageHandler& operator=(const BrowserPageHandler&) = delete;
  ~BrowserPageHandler() override;

  static void CreateForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver);

  // webui_examples::mojom::PageHandler
  void CreateGuestView(base::Value::Dict create_params,
                       CreateGuestViewCallback callback) override;
  void Navigate(int32_t guest_instance_id, const GURL& src) override;
  void GoBack(int32_t guest_instance_id) override;
  void GoForward(int32_t guest_instance_id) override;

 private:
  BrowserPageHandler(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver);
};

}  // namespace webui_examples

#endif  //  UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_PAGE_HANDLER_H_
