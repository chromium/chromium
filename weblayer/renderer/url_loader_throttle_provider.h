// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_
#define WEBLAYER_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_

#include "base/threading/thread_checker.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

namespace weblayer {

// Instances must be constructed on the render thread, and then used and
// destructed on a single thread, which can be different from the render thread.
class URLLoaderThrottleProvider : public blink::URLLoaderThrottleProvider {
 public:
  URLLoaderThrottleProvider(
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker,
      blink::URLLoaderThrottleProviderType type);

  URLLoaderThrottleProvider& operator=(const URLLoaderThrottleProvider&) =
      delete;

  ~URLLoaderThrottleProvider() override;

  // blink::URLLoaderThrottleProvider implementation.
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  URLLoaderThrottleProvider(const URLLoaderThrottleProvider& other);

  blink::URLLoaderThrottleProviderType type_;

  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing> safe_browsing_remote_;
  mojo::Remote<safe_browsing::mojom::SafeBrowsing> safe_browsing_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_
