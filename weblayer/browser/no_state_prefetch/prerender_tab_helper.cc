// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/prerender_tab_helper.h"

#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"

namespace weblayer {

PrerenderTabHelper::PrerenderTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PrerenderTabHelper>(*web_contents) {}

PrerenderTabHelper::~PrerenderTabHelper() = default;

void PrerenderTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return;
  }

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  if (no_state_prefetch_manager &&
      !no_state_prefetch_manager->IsWebContentsPrefetching(web_contents()))
    no_state_prefetch_manager->RecordNavigation(navigation_handle->GetURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrerenderTabHelper);

}  // namespace weblayer
