// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_TAB_HELPER_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace prerender {
class NoStatePrefetchManager;
}

namespace weblayer {

// Notifies the prerender::NoStatePrefetchManager with the events happening in
// the prerendered WebContents.
class PrerenderTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PrerenderTabHelper> {
 public:
  ~PrerenderTabHelper() override;
  PrerenderTabHelper(const PrerenderTabHelper&) = delete;
  PrerenderTabHelper& operator=(const PrerenderTabHelper&) = delete;

  // content::WebContentsObserver implementation.
  void PrimaryPageChanged(content::Page& page) override;

 private:
  explicit PrerenderTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrerenderTabHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_TAB_HELPER_H_