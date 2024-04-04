// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/renderer/wolvic_content_renderer_client.h"

#include "components/cdm/renderer/key_system_support_update.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "wolvic/renderer/browser_exposed_renderer_interfaces.h"
#include "wolvic/renderer/wolvic_render_frame_observer.h"

namespace wolvic {

WolvicContentRendererClient::WolvicContentRendererClient() = default;

WolvicContentRendererClient::~WolvicContentRendererClient() = default;

void WolvicContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  cdm::GetSupportedKeySystemsUpdates(/*can_persist_data=*/true, std::move(cb));
}

void WolvicContentRendererClient::RenderThreadStarted() {
  visited_link_reader_ = std::make_unique<visitedlink::VisitedLinkReader>();
}

void WolvicContentRendererClient::ExposeInterfacesToBrowser(mojo::BinderMap* binders) {
  // NOTE: Do not add binders directly within this method. Instead, modify the
  // definition of |ExposeRendererInterfacesToBrowser()| to ensure security
  // review coverage.
  ExposeRendererInterfacesToBrowser(this, binders);
}

void WolvicContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  auto* render_frame_observer = new WolvicRenderFrameObserver(render_frame);
  blink::AssociatedInterfaceRegistry* associated_interfaces =
      render_frame_observer->associated_interfaces();

  if (!render_frame->IsInFencedFrameTree() ||
      base::FeatureList::IsEnabled(blink::features::kFencedFramesAPIChanges)) {
    auto* password_autofill_agent =
        new autofill::PasswordAutofillAgent(render_frame, associated_interfaces);
    new autofill::AutofillAgent(render_frame, password_autofill_agent,
                      nullptr, associated_interfaces);
  }
}

uint64_t WolvicContentRendererClient::VisitedLinkHash(const char* canonical_url,
						      size_t length) {
  return visited_link_reader_->ComputeURLFingerprint(canonical_url, length);
}

bool WolvicContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return visited_link_reader_->IsVisited(link_hash);
}

}  // namespace wolvic
