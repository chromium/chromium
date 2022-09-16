// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/no_state_prefetch_utils.h"

#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"

namespace weblayer {
prerender::NoStatePrefetchContents* NoStatePrefetchContentsFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!no_state_prefetch_manager)
    return nullptr;

  return no_state_prefetch_manager->GetNoStatePrefetchContents(web_contents);
}

}  // namespace weblayer
