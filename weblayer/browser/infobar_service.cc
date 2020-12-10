// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/infobar_service.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/bind.h"
#include "components/infobars/android/confirm_infobar.h"
#endif

namespace weblayer {

InfoBarService::InfoBarService(content::WebContents* web_contents)
    : infobars::ContentInfoBarManager(web_contents) {}

InfoBarService::~InfoBarService() {}

#if defined(OS_ANDROID)
std::unique_ptr<infobars::InfoBar> InfoBarService::CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  return std::make_unique<infobars::ConfirmInfoBar>(std::move(delegate));
}
#endif  // if defined(OS_ANDROID)

void InfoBarService::WebContentsDestroyed() {
  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add infobars or use
  // us otherwise during the destruction.
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(InfoBarService)

}  // namespace weblayer
