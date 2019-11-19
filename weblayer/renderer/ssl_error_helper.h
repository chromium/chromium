// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_SSL_ERROR_HELPER_H_
#define WEBLAYER_RENDERER_SSL_ERROR_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/common/mojom/interstitial_commands.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace weblayer {

// A class that helps present SSL interstitials by enabling security
// interstitial javascript in WebContents that have navigation errors. This is a
// stripped down version of Chrome's NetErrorHelper.
class SSLErrorHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<SSLErrorHelper>,
      public security_interstitials::SecurityInterstitialPageController::
          Delegate {
 public:
  // Creates an SSLErrorHelper which will observe and tie its lifetime to
  // |render_frame|, if it's a main frame. SSLErrorHelpers will not be created
  // for sub frames.
  static void Create(content::RenderFrame* render_frame);

  // Returns the SSLErrorHelper for the frame, if it exists.
  static SSLErrorHelper* GetForFrame(content::RenderFrame* render_frame);

  // Called when the current navigation results in an error.
  void PrepareErrorPage();

  // content::RenderFrameObserver:
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidFinishLoad() override;
  void OnDestruct() override;

  // security_interstitials::SecurityInterstitialPageController::Delegate:
  void SendCommand(
      security_interstitials::SecurityInterstitialCommand command) override;

  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
  GetInterface() override;

 private:
  explicit SSLErrorHelper(content::RenderFrame* render_frame);
  ~SSLErrorHelper() override;

  bool next_load_is_error_ = false;
  bool this_load_is_error_ = false;

  base::WeakPtrFactory<SSLErrorHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHelper);
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_SSL_ERROR_HELPER_H_
