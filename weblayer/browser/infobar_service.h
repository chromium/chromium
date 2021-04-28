// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_INFOBAR_SERVICE_H_
#define WEBLAYER_BROWSER_INFOBAR_SERVICE_H_

#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace weblayer {

// WebLayer's specialization of ContentInfoBarManager, which ties the lifetime
// of ContentInfoBarManager instances to that of the WebContents with which they
// are associated.
class InfoBarService : public infobars::ContentInfoBarManager,
                       public content::WebContentsUserData<InfoBarService> {
 public:
  ~InfoBarService() override;
  InfoBarService(const InfoBarService&) = delete;
  InfoBarService& operator=(const InfoBarService&) = delete;

 protected:
  explicit InfoBarService(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<InfoBarService>;

  // infobars::ContentInfoBarManager:
  void WebContentsDestroyed() override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_INFOBAR_SERVICE_H_
