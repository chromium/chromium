// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/weblayer_render_frame_observer.h"

#include "content/public/renderer/render_frame.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "components/translate/core/common/translate_util.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "weblayer/common/features.h"
#include "weblayer/common/isolated_world_ids.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier_delegate.h"
#endif

namespace weblayer {

namespace {
// Maximum number of characters in the document to index.
// Any text beyond this point will be clipped.
static const size_t kMaxIndexChars = 65535;

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

// For a page that auto-refreshes, we still show the bubble, if
// the refresh delay is less than this value (in seconds).
static constexpr base::TimeDelta kLocationChangeInterval = base::Seconds(10);
}  // namespace

WebLayerRenderFrameObserver::WebLayerRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame), translate_agent_(nullptr) {
  // Don't do anything for subframes.
  if (!render_frame->IsMainFrame())
    return;

  if (base::FeatureList::IsEnabled(
          features::kWebLayerClientSidePhishingDetection))
    SetClientSidePhishingDetection();

  // TODO(crbug.com/1073370): Handle case where subframe translation is enabled.
  DCHECK(!translate::IsSubFrameTranslationEnabled());
  translate_agent_ =
      new translate::TranslateAgent(render_frame, ISOLATED_WORLD_ID_TRANSLATE);
}

WebLayerRenderFrameObserver::~WebLayerRenderFrameObserver() = default;

bool WebLayerRenderFrameObserver::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return associated_interfaces_.TryBindInterface(interface_name, handle);
}

void WebLayerRenderFrameObserver::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  // Let translate_agent do any preparatory work for loading a URL.
  if (!translate_agent_)
    return;

  translate_agent_->PrepareForUrl(
      render_frame()->GetWebFrame()->GetDocument().Url());
}

void WebLayerRenderFrameObserver::DidMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  // Don't do any work for subframes.
  if (!render_frame()->IsMainFrame())
    return;

  switch (layout_type) {
    case blink::WebMeaningfulLayout::kFinishedParsing:
      CapturePageText(PRELIMINARY_CAPTURE);
      break;
    case blink::WebMeaningfulLayout::kFinishedLoading:
      CapturePageText(FINAL_CAPTURE);
      break;
    default:
      break;
  }
}

// NOTE: This is a simplified version of
// ChromeRenderFrameObserver::CapturePageText(), which is more complex as it is
// used for embedder-level purposes beyond translation. This code is expected to
// be eliminated when WebLayer adopts Chrome's upcoming per-frame translate
// architecture (crbug.com/1063520).
void WebLayerRenderFrameObserver::CapturePageText(
    TextCaptureType capture_type) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;

  // Don't capture pages that have pending redirect or location change.
  if (frame->IsNavigationScheduledWithin(kLocationChangeInterval))
    return;

  // Don't index/capture pages that are in view source mode.
  if (frame->IsViewSourceModeEnabled())
    return;

  // Don't capture text of the error pages.
  blink::WebDocumentLoader* document_loader = frame->GetDocumentLoader();
  if (document_loader && document_loader->HasUnreachableURL())
    return;

  // Don't index/capture pages that are being no-state prefetched.
  if (prerender::NoStatePrefetchHelper::IsPrefetching(render_frame()))
    return;

    // Don't capture contents unless there is either a translate agent or a
    // phishing classifier to consume them.
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (!translate_agent_ && !phishing_classifier_)
    return;
#else
  if (!translate_agent_)
    return;
#endif

  base::TimeTicks capture_begin_time = base::TimeTicks::Now();

  // Retrieve the frame's full text (up to kMaxIndexChars), and pass it to the
  // translate helper for language detection and possible translation.
  // TODO(http://crbug.com/1163244): Update this when the corresponding usage of
  // this function in //chrome is updated.
  std::u16string contents =
      blink::WebFrameContentDumper::DumpFrameTreeAsText(frame, kMaxIndexChars)
          .Utf16();

  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeTicks::Now() - capture_begin_time);

  // We should run language detection only once. Parsing finishes before
  // the page loads, so let's pick that timing (as in chrome).
  if (translate_agent_ && capture_type == PRELIMINARY_CAPTURE) {
    translate_agent_->PageCaptured(contents);
  }

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (phishing_classifier_)
    phishing_classifier_->PageCaptured(&contents,
                                       capture_type == PRELIMINARY_CAPTURE);
#endif
}

void WebLayerRenderFrameObserver::OnDestruct() {
  delete this;
}

void WebLayerRenderFrameObserver::SetClientSidePhishingDetection() {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  phishing_classifier_ = safe_browsing::PhishingClassifierDelegate::Create(
      render_frame(), nullptr);
#endif
}

}  // namespace weblayer
