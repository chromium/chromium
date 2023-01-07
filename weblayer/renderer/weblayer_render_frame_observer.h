// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_WEBLAYER_RENDER_FRAME_OBSERVER_H_
#define WEBLAYER_RENDERER_WEBLAYER_RENDER_FRAME_OBSERVER_H_

#include "components/safe_browsing/buildflags.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace safe_browsing {
class PhishingClassifierDelegate;
}

namespace translate {
class TranslateAgent;
}

namespace weblayer {

// This class holds the WebLayer-specific parts of RenderFrame, and has the
// same lifetime. It is analogous to //chrome's ChromeRenderFrameObserver.
class WebLayerRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit WebLayerRenderFrameObserver(content::RenderFrame* render_frame);

  WebLayerRenderFrameObserver(const WebLayerRenderFrameObserver&) = delete;
  WebLayerRenderFrameObserver& operator=(const WebLayerRenderFrameObserver&) =
      delete;

  blink::AssociatedInterfaceRegistry* associated_interfaces() {
    return &associated_interfaces_;
  }

 private:
  enum TextCaptureType { PRELIMINARY_CAPTURE, FINAL_CAPTURE };
  ~WebLayerRenderFrameObserver() override;

  // RenderFrameObserver:
  bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void OnDestruct() override;

  void CapturePageText(TextCaptureType capture_type);

  // Initializes a |phishing_classifier_delegate_|.
  void SetClientSidePhishingDetection();

  blink::AssociatedInterfaceRegistry associated_interfaces_;

  // Has the same lifetime as this object.
  translate::TranslateAgent* translate_agent_;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::PhishingClassifierDelegate* phishing_classifier_ = nullptr;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_WEBLAYER_RENDER_FRAME_OBSERVER_H_
