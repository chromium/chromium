// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/ssl_error_helper.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace weblayer {

// static
void SSLErrorHelper::Create(content::RenderFrame* render_frame) {
  if (render_frame->IsMainFrame())
    new SSLErrorHelper(render_frame);
}

// static
SSLErrorHelper* SSLErrorHelper::GetForFrame(
    content::RenderFrame* render_frame) {
  return render_frame->IsMainFrame() ? Get(render_frame) : nullptr;
}

void SSLErrorHelper::PrepareErrorPage() {
  next_load_is_error_ = true;
}

void SSLErrorHelper::DidCommitProvisionalLoad(bool is_same_document_navigation,
                                              ui::PageTransition transition) {
  if (is_same_document_navigation)
    return;

  weak_factory_.InvalidateWeakPtrs();
  this_load_is_error_ = next_load_is_error_;
  next_load_is_error_ = false;
}

void SSLErrorHelper::DidFinishLoad() {
  if (this_load_is_error_) {
    security_interstitials::SecurityInterstitialPageController::Install(
        render_frame(), weak_factory_.GetWeakPtr());
  }
}

void SSLErrorHelper::OnDestruct() {
  delete this;
}

void SSLErrorHelper::SendCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
      interface = GetInterface();
  switch (command) {
    case security_interstitials::CMD_DONT_PROCEED:
      interface->DontProceed();
      break;
    case security_interstitials::CMD_PROCEED:
      interface->Proceed();
      break;
    case security_interstitials::CMD_SHOW_MORE_SECTION:
      interface->ShowMoreSection();
      break;
    case security_interstitials::CMD_OPEN_HELP_CENTER:
      interface->OpenHelpCenter();
      break;
    case security_interstitials::CMD_RELOAD:
      interface->Reload();
      break;
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
    case security_interstitials::CMD_OPEN_LOGIN:
    case security_interstitials::CMD_DO_REPORT:
    case security_interstitials::CMD_DONT_REPORT:
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
    case security_interstitials::CMD_OPEN_WHITEPAPER:
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      // Commands not used by the generic SSL error page.
      NOTREACHED();
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands for testing.
      NOTREACHED();
      break;
  }
}

mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
SSLErrorHelper::GetInterface() {
  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
      interface;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&interface);
  return interface;
}

SSLErrorHelper::SSLErrorHelper(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<SSLErrorHelper>(render_frame) {}

SSLErrorHelper::~SSLErrorHelper() = default;

}  // namespace weblayer
