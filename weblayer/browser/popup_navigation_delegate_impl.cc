// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/popup_navigation_delegate_impl.h"

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/blocked_content/android/popup_blocked_infobar_delegate.h"
#endif

namespace weblayer {

PopupNavigationDelegateImpl::PopupNavigationDelegateImpl(
    const content::OpenURLParams& params,
    content::WebContents* source_contents,
    content::RenderFrameHost* opener)
    : params_(params),
      source_contents_(source_contents),
      opener_(opener),
      original_user_gesture_(params_.user_gesture) {}

content::RenderFrameHost* PopupNavigationDelegateImpl::GetOpener() {
  return opener_;
}

bool PopupNavigationDelegateImpl::GetOriginalUserGesture() {
  return original_user_gesture_;
}

GURL PopupNavigationDelegateImpl::GetURL() {
  return params_.url;
}

blocked_content::PopupNavigationDelegate::NavigateResult
PopupNavigationDelegateImpl::NavigateWithGesture(
    const blink::mojom::WindowFeatures& window_features,
    absl::optional<WindowOpenDisposition> updated_disposition) {
  // It's safe to mutate |params_| here because NavigateWithGesture() will only
  // be called once, and the user gesture value has already been saved in
  // |original_user_gesture_|.
  params_.user_gesture = true;
  if (updated_disposition)
    params_.disposition = updated_disposition.value();
  content::WebContents* new_contents = source_contents_->OpenURL(params_);
  return NavigateResult{
      new_contents,
      params_.disposition,
  };
}

void PopupNavigationDelegateImpl::OnPopupBlocked(
    content::WebContents* web_contents,
    int total_popups_blocked_on_page) {
#if BUILDFLAG(IS_ANDROID)
  blocked_content::PopupBlockedInfoBarDelegate::Create(
      infobars::ContentInfoBarManager::FromWebContents(web_contents),
      total_popups_blocked_on_page,
      HostContentSettingsMapFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()),
      base::NullCallback());
#endif
}

}  // namespace weblayer
