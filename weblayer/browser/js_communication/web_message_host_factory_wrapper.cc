// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/js_communication/web_message_host_factory_wrapper.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/js_injection/browser/web_message.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/browser/web_message_reply_proxy.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "weblayer/browser/page_impl.h"
#include "weblayer/public/js_communication/web_message.h"
#include "weblayer/public/js_communication/web_message_host.h"
#include "weblayer/public/js_communication/web_message_host_factory.h"
#include "weblayer/public/js_communication/web_message_reply_proxy.h"

namespace weblayer {
namespace {

// An implementation of js_injection::WebMessageHost that delegates to the
// corresponding WebLayer type. This also serves as the WebMessageReplyProxy
// implementation, which forwards to the js_injection implementation.
class WebMessageHostWrapper : public js_injection::WebMessageHost,
                              public WebMessageReplyProxy {
 public:
  WebMessageHostWrapper(weblayer::WebMessageHostFactory* factory,
                        const std::string& origin_string,
                        bool is_main_frame,
                        js_injection::WebMessageReplyProxy* proxy)
      : proxy_(proxy),
        connection_(factory->CreateHost(origin_string, is_main_frame, this)) {}

  // js_injection::WebMessageHost:
  void OnPostMessage(
      std::unique_ptr<js_injection::WebMessage> message) override {
    std::unique_ptr<WebMessage> m = std::make_unique<WebMessage>();
    auto& payload = message->message;
    if (!absl::holds_alternative<std::u16string>(payload)) {
      // Ignore non-string messages, not supported by weblayer.
      return;
    }
    m->message = std::move(absl::get<std::u16string>(payload));
    connection_->OnPostMessage(std::move(m));
  }
  void OnBackForwardCacheStateChanged() override {
    connection_->OnBackForwardCacheStateChanged();
  }

  // WebMessageReplyProxy:
  void PostWebMessage(std::unique_ptr<WebMessage> message) override {
    proxy_->PostWebMessage(std::move(message->message));
  }
  bool IsInBackForwardCache() override {
    return proxy_->IsInBackForwardCache();
  }
  Page& GetPage() override {
    // In general WebLayer avoids exposing child frames. As such, GetPage()
    // returns the Page of the main frame.
    PageImpl* page =
        PageImpl::GetForPage(proxy_->GetPage().GetMainDocument().GetPage());
    // NavigationControllerImpl creates the PageImpl when navigation finishes so
    // that by the time this is called the Page should have been created.
    DCHECK(page);
    return *page;
  }

 private:
  raw_ptr<js_injection::WebMessageReplyProxy> proxy_;
  std::unique_ptr<weblayer::WebMessageHost> connection_;
};

}  // namespace

WebMessageHostFactoryWrapper::WebMessageHostFactoryWrapper(
    std::unique_ptr<weblayer::WebMessageHostFactory> factory)
    : factory_(std::move(factory)) {}

WebMessageHostFactoryWrapper::~WebMessageHostFactoryWrapper() = default;

std::unique_ptr<js_injection::WebMessageHost>
WebMessageHostFactoryWrapper::CreateHost(
    const std::string& origin_string,
    bool is_main_frame,
    js_injection::WebMessageReplyProxy* proxy) {
  auto wrapper = std::make_unique<WebMessageHostWrapper>(
      factory_.get(), origin_string, is_main_frame, proxy);
  return wrapper;
}

}  // namespace weblayer
