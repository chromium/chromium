// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/prerender_processor_impl_delegate_impl.h"

#include "components/no_state_prefetch/browser/prerender_link_manager.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/no_state_prefetch/prerender_link_manager_factory.h"

namespace weblayer {

prerender::PrerenderLinkManager*
PrerenderProcessorImplDelegateImpl::GetPrerenderLinkManager(
    content::BrowserContext* browser_context) {
  return PrerenderLinkManagerFactory::GetForBrowserContext(browser_context);
}

}  // namespace weblayer
