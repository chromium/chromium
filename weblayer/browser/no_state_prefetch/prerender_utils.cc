// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/prerender_utils.h"

#include "components/no_state_prefetch/browser/prerender_contents.h"
#include "components/no_state_prefetch/browser/prerender_manager.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"

namespace weblayer {
prerender::PrerenderContents* PrerenderContentsFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  prerender::PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!prerender_manager)
    return nullptr;

  return prerender_manager->GetPrerenderContents(web_contents);
}

}  // namespace weblayer
