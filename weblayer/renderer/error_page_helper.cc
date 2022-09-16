// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/error_page_helper.h"

#include "base/command_line.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "weblayer/common/features.h"

namespace weblayer {

// static
void ErrorPageHelper::Create(content::RenderFrame* render_frame) {
  if (render_frame->IsMainFrame())
    new ErrorPageHelper(render_frame);
}

// static
ErrorPageHelper* ErrorPageHelper::GetForFrame(
    content::RenderFrame* render_frame) {
  return render_frame->IsMainFrame() ? Get(render_frame) : nullptr;
}

void ErrorPageHelper::PrepareErrorPage() {
  if (is_disabled_for_next_error_) {
    is_disabled_for_next_error_ = false;
    return;
  }
  is_preparing_for_error_page_ = true;
}

void ErrorPageHelper::DidCommitProvisionalLoad(ui::PageTransition transition) {
  show_error_page_in_finish_load_ = is_preparing_for_error_page_;
  is_preparing_for_error_page_ = false;
}

void ErrorPageHelper::DidFinishLoad() {
  if (!show_error_page_in_finish_load_)
    return;

  security_interstitials::SecurityInterstitialPageController::Install(
      render_frame());
}

void ErrorPageHelper::OnDestruct() {
  delete this;
}

void ErrorPageHelper::DisableErrorPageHelperForNextError() {
  is_disabled_for_next_error_ = true;
}

ErrorPageHelper::ErrorPageHelper(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<ErrorPageHelper>(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::ErrorPageHelper>(base::BindRepeating(
          &ErrorPageHelper::BindErrorPageHelper, weak_factory_.GetWeakPtr()));
}

ErrorPageHelper::~ErrorPageHelper() = default;

void ErrorPageHelper::BindErrorPageHelper(
    mojo::PendingAssociatedReceiver<mojom::ErrorPageHelper> receiver) {
  // There is only a need for a single receiver to be bound at a time.
  error_page_helper_receiver_.reset();
  error_page_helper_receiver_.Bind(std::move(receiver));
}

}  // namespace weblayer
