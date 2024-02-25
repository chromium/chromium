// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_GUEST_VIEW_MANAGER_DELEGATE_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_GUEST_VIEW_MANAGER_DELEGATE_H_

#include "components/guest_view/browser/guest_view_manager_delegate.h"

namespace webui_examples {

class GuestViewManagerDelegate : public guest_view::GuestViewManagerDelegate {
 public:
  GuestViewManagerDelegate();
  ~GuestViewManagerDelegate() override;

  // guest_view::GuestViewManagerDelegate:
  void OnGuestAdded(content::WebContents* guest_web_contents) const override;
  void DispatchEvent(const std::string& event_name,
                     base::Value::Dict args,
                     guest_view::GuestViewBase* guest,
                     int instance_id) override;
  bool IsGuestAvailableToContext(
      const guest_view::GuestViewBase* guest) const override;
  bool IsOwnedByExtension(const guest_view::GuestViewBase* guest) override;
  void RegisterAdditionalGuestViewTypes(
      guest_view::GuestViewManager* manager) override;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_GUEST_VIEW_MANAGER_DELEGATE_H_
