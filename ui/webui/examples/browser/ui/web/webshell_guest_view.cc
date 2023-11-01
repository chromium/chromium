// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/web/webshell_guest_view.h"

#include <memory>

#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "ui/webui/examples/browser/ui/web/guest_view_manager_delegate.h"

namespace webui_examples {

WebshellGuestView::WebshellGuestView(
    const content::GlobalRenderFrameHostId& frame_id,
    base::PassKey<WebshellGuestView>)
    : guest_view::GuestViewMessageHandler(frame_id) {}

WebshellGuestView::~WebshellGuestView() = default;

// static
void WebshellGuestView::Create(
    const content::GlobalRenderFrameHostId& frame_id,
    mojo::PendingAssociatedReceiver<guest_view::mojom::GuestViewHost>
        receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<WebshellGuestView>(frame_id,
                                          base::PassKey<WebshellGuestView>()),
      std::move(receiver));
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
WebshellGuestView::CreateGuestViewManagerDelegate() const {
  return std::make_unique<GuestViewManagerDelegate>();
}

}  // namespace webui_examples
