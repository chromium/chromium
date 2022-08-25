// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/url_bar/page_info_delegate_impl.h"

#include "build/build_config.h"
#include "components/permissions/permission_util.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_state/content/content_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "url/origin.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/page_specific_content_settings_delegate.h"
#include "weblayer/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "weblayer/browser/stateful_ssl_host_state_delegate_factory.h"
#include "weblayer/browser/subresource_filter_profile_context_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "weblayer/browser/weblayer_impl_android.h"
#endif

namespace weblayer {

PageInfoDelegateImpl::PageInfoDelegateImpl(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);
}

permissions::ObjectPermissionContextBase*
PageInfoDelegateImpl::GetChooserContext(ContentSettingsType type) {
  // TODO(crbug.com/1052375): Once WebLayer has USB and Bluetooth support,
  // add more logic here.
  return nullptr;
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
safe_browsing::PasswordProtectionService*
PageInfoDelegateImpl::GetPasswordProtectionService() const {
  NOTREACHED();
  return nullptr;
}

void PageInfoDelegateImpl::OnUserActionOnPasswordUi(
    safe_browsing::WarningAction action) {
  NOTREACHED();
}

std::u16string PageInfoDelegateImpl::GetWarningDetailText() {
  // TODO(crbug.com/1052375): Implement.
  NOTREACHED();
  return std::u16string();
}
#endif

permissions::PermissionResult PageInfoDelegateImpl::GetPermissionResult(
    blink::PermissionType permission,
    const url::Origin& origin) {
  content::PermissionResult permission_result =
      GetBrowserContext()
          ->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(permission, origin);
  return permissions::PermissionUtil::ToPermissionResult(permission_result);
}

#if !BUILDFLAG(IS_ANDROID)
bool PageInfoDelegateImpl::CreateInfoBarDelegate() {
  NOTREACHED();
  return false;
}

void PageInfoDelegateImpl::ShowSiteSettings(const GURL& site_url) {
  // TODO(crbug.com/1052375): Implement once site settings code has been
  // componentized.
  NOTREACHED();
}

void PageInfoDelegateImpl::ShowCookiesSettings() {
  // Used for desktop only. Doesn't need implementation for WebLayer.
  NOTREACHED();
}

void PageInfoDelegateImpl::OpenCookiesDialog() {
  // Used for desktop only. Doesn't need implementation for WebLayer.
  NOTREACHED();
}

void PageInfoDelegateImpl::OpenCertificateDialog(
    net::X509Certificate* certificate) {
  // Used for desktop only. Doesn't need implementation for WebLayer.
  NOTREACHED();
}

void PageInfoDelegateImpl::OpenConnectionHelpCenterPage(
    const ui::Event& event) {
  // Used for desktop only. Doesn't need implementation for WebLayer.
  NOTREACHED();
}

void PageInfoDelegateImpl::OpenSafetyTipHelpCenterPage() {
  // Used for desktop only. Doesn't need implementation for WebLayer.
  NOTREACHED();
}

void PageInfoDelegateImpl::OpenContentSettingsExceptions(
    ContentSettingsType content_settings_type) {
  // Used for desktop only. Doesn't need implementation for WebLayer.
  NOTREACHED();
}

void PageInfoDelegateImpl::OnPageInfoActionOccurred(
    PageInfo::PageInfoAction action) {
  // Used for desktop only. Doesn't need implementation for WebLayer.
  NOTREACHED();
}

void PageInfoDelegateImpl::OnUIClosing() {
  // Used for desktop only. Doesn't need implementation for WebLayer.
  NOTREACHED();
}
#endif

permissions::PermissionDecisionAutoBlocker*
PageInfoDelegateImpl::GetPermissionDecisionAutoblocker() {
  return PermissionDecisionAutoBlockerFactory::GetForBrowserContext(
      GetBrowserContext());
}

StatefulSSLHostStateDelegate*
PageInfoDelegateImpl::GetStatefulSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetInstance()
      ->GetForBrowserContext(GetBrowserContext());
}

HostContentSettingsMap* PageInfoDelegateImpl::GetContentSettings() {
  return HostContentSettingsMapFactory::GetForBrowserContext(
      GetBrowserContext());
}

bool PageInfoDelegateImpl::IsSubresourceFilterActivated(const GURL& site_url) {
  return SubresourceFilterProfileContextFactory::GetForBrowserContext(
             GetBrowserContext())
      ->settings_manager()
      ->GetSiteActivationFromMetadata(site_url);
}

bool PageInfoDelegateImpl::IsContentDisplayedInVrHeadset() {
  // VR is not supported for WebLayer.
  return false;
}

security_state::SecurityLevel PageInfoDelegateImpl::GetSecurityLevel() {
  auto state = security_state::GetVisibleSecurityState(web_contents_);
  DCHECK(state);
  return security_state::GetSecurityLevel(
      *state,
      /* used_policy_installed_certificate */ false);
}

security_state::VisibleSecurityState
PageInfoDelegateImpl::GetVisibleSecurityState() {
  return *security_state::GetVisibleSecurityState(web_contents_);
}

std::unique_ptr<content_settings::PageSpecificContentSettings::Delegate>
PageInfoDelegateImpl::GetPageSpecificContentSettingsDelegate() {
  return std::make_unique<PageSpecificContentSettingsDelegate>(web_contents_);
}

#if BUILDFLAG(IS_ANDROID)
const std::u16string PageInfoDelegateImpl::GetClientApplicationName() {
  return weblayer::GetClientApplicationName();
}
#endif

content::BrowserContext* PageInfoDelegateImpl::GetBrowserContext() const {
  return web_contents_->GetBrowserContext();
}

}  //  namespace weblayer
