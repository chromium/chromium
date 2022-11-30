// Copyright 2020 The Chromium Authors
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

void PrerenderTabHelper::PrimaryPageChanged(content::Page& page) {
  if (page.GetMainDocument().IsErrorDocument())
    return;

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  if (no_state_prefetch_manager &&
      !no_state_prefetch_manager->IsWebContentsPrefetching(web_contents()))
    no_state_prefetch_manager->RecordNavigation(
        page.GetMainDocument().GetLastCommittedURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrerenderTabHelper);

}  // namespace weblayer
