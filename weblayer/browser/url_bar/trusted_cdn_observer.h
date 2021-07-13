// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_URL_BAR_TRUSTED_CDN_OBSERVER_H_
#define WEBLAYER_BROWSER_URL_BAR_TRUSTED_CDN_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace weblayer {

// Monitors navigations to see if a publisher URL exists.
class TrustedCDNObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TrustedCDNObserver> {
 public:
  ~TrustedCDNObserver() override;

  const GURL& publisher_url() const { return publisher_url_; }

 private:
  friend class content::WebContentsUserData<TrustedCDNObserver>;

  explicit TrustedCDNObserver(content::WebContents* web_contents);

  // content::WebContentsObserver implementation:
  void PrimaryPageChanged(content::Page& page) override;

  GURL publisher_url_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_URL_BAR_TRUSTED_CDN_OBSERVER_H_