// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/web/guest_view_manager_delegate.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "ui/webui/examples/browser/ui/web/web_view.h"

namespace webui_examples {

GuestViewManagerDelegate::GuestViewManagerDelegate() = default;

GuestViewManagerDelegate::~GuestViewManagerDelegate() = default;

void GuestViewManagerDelegate::OnGuestAdded(
    content::WebContents* guest_web_contents) const {}

void GuestViewManagerDelegate::DispatchEvent(const std::string& event_name,
                                             base::Value::Dict args,
                                             guest_view::GuestViewBase* guest,
                                             int instance_id) {}

bool GuestViewManagerDelegate::IsGuestAvailableToContext(
    const guest_view::GuestViewBase* guest) const {
  // Verify that we're only running this in a WebUI.
  CHECK(guest->owner_rfh()->GetMainFrame()->GetWebUI());
  return true;
}

bool GuestViewManagerDelegate::IsOwnedByExtension(
    const guest_view::GuestViewBase* guest) {
  return false;
}

void GuestViewManagerDelegate::RegisterAdditionalGuestViewTypes(
    guest_view::GuestViewManager* manager) {
  manager->RegisterGuestViewType(
      WebView::Type, base::BindRepeating(&WebView::Create), base::DoNothing());
}

}  // namespace webui_examples
