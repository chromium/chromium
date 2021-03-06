// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_URL_BAR_PAGE_INFO_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_URL_BAR_PAGE_INFO_DELEGATE_IMPL_H_

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/page_info/page_info_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace weblayer {

class PageInfoDelegateImpl : public PageInfoDelegate {
 public:
  explicit PageInfoDelegateImpl(content::WebContents* web_contents);
  ~PageInfoDelegateImpl() override = default;

  // PageInfoDelegate implementation
  permissions::ChooserContextBase* GetChooserContext(
      ContentSettingsType type) override;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;
  void OnUserActionOnPasswordUi(content::WebContents* web_contents,
                                safe_browsing::WarningAction action) override;
  base::string16 GetWarningDetailText() override;
#endif
  permissions::PermissionResult GetPermissionStatus(
      ContentSettingsType type,
      const GURL& site_url) override;

#if !defined(OS_ANDROID)
  bool CreateInfoBarDelegate() override;
  void ShowSiteSettings(const GURL& site_url) override;
#endif

  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoblocker()
      override;
  StatefulSSLHostStateDelegate* GetStatefulSSLHostStateDelegate() override;
  HostContentSettingsMap* GetContentSettings() override;
  std::unique_ptr<content_settings::PageSpecificContentSettings::Delegate>
  GetPageSpecificContentSettingsDelegate() override;
  bool IsSubresourceFilterActivated(const GURL& site_url) override;
  bool IsContentDisplayedInVrHeadset() override;
  security_state::SecurityLevel GetSecurityLevel() override;
  security_state::VisibleSecurityState GetVisibleSecurityState() override;

#if defined(OS_ANDROID)
  const base::string16 GetClientApplicationName() override;
#endif

 private:
  content::BrowserContext* GetBrowserContext() const;

  content::WebContents* web_contents_;
};

}  //  namespace weblayer

#endif  // WEBLAYER_BROWSER_URL_BAR_PAGE_INFO_DELEGATE_IMPL_H_
