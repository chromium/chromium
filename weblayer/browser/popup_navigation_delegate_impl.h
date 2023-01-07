// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_POPUP_NAVIGATION_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_POPUP_NAVIGATION_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"

namespace weblayer {

class PopupNavigationDelegateImpl
    : public blocked_content::PopupNavigationDelegate {
 public:
  PopupNavigationDelegateImpl(const content::OpenURLParams& params,
                              content::WebContents* source_contents,
                              content::RenderFrameHost* opener);

  // blocked_content::PopupNavigationDelegate:
  content::RenderFrameHost* GetOpener() override;
  bool GetOriginalUserGesture() override;
  const GURL& GetURL() override;
  NavigateResult NavigateWithGesture(
      const blink::mojom::WindowFeatures& window_features,
      absl::optional<WindowOpenDisposition> updated_disposition) override;
  void OnPopupBlocked(content::WebContents* web_contents,
                      int total_popups_blocked_on_page) override;

  const content::OpenURLParams& params() const { return params_; }

 private:
  content::OpenURLParams params_;
  raw_ptr<content::WebContents> source_contents_;
  raw_ptr<content::RenderFrameHost> opener_;
  const bool original_user_gesture_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_POPUP_NAVIGATION_DELEGATE_IMPL_H_
