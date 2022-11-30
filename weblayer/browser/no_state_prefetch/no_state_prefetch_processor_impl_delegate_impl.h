// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_PROCESSOR_IMPL_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_PROCESSOR_IMPL_DELEGATE_IMPL_H_

#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl_delegate.h"

namespace content {
class BrowserContext;
}

namespace prerender {
class NoStatePrefetchLinkManager;
}

namespace weblayer {

class NoStatePrefetchProcessorImplDelegateImpl
    : public prerender::NoStatePrefetchProcessorImplDelegate {
 public:
  NoStatePrefetchProcessorImplDelegateImpl() = default;
  ~NoStatePrefetchProcessorImplDelegateImpl() override = default;

  // prerender::NoStatePrefetchProcessorImplDelegate overrides,
  prerender::NoStatePrefetchLinkManager* GetNoStatePrefetchLinkManager(
      content::BrowserContext* browser_context) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_PROCESSOR_IMPL_DELEGATE_IMPL_H_
