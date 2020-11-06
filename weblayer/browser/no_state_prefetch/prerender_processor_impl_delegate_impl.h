// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_PROCESSOR_IMPL_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_PROCESSOR_IMPL_DELEGATE_IMPL_H_

#include "components/no_state_prefetch/browser/prerender_processor_impl_delegate.h"

namespace content {
class BrowserContext;
}

namespace prerender {
class PrerenderLinkManager;
}

namespace weblayer {

class PrerenderProcessorImplDelegateImpl
    : public prerender::PrerenderProcessorImplDelegate {
 public:
  PrerenderProcessorImplDelegateImpl() = default;
  ~PrerenderProcessorImplDelegateImpl() override = default;

  // prerender::PrerenderProcessorImplDelegate overrides,
  prerender::PrerenderLinkManager* GetPrerenderLinkManager(
      content::BrowserContext* browser_context) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_PROCESSOR_IMPL_DELEGATE_IMPL_H_
