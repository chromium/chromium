// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/weblayer_render_frame_observer.h"

#include "content/public/renderer/render_frame.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/no_state_prefetch/renderer/prerender_helper.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "components/translate/core/common/translate_util.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "weblayer/common/isolated_world_ids.h"

namespace weblayer {

namespace {
// Maximum number of characters in the document to index.
// Any text beyond this point will be clipped.
static const size_t kMaxIndexChars = 65535;

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

// For a page that auto-refreshes, we still show the bubble, if
// the refresh delay is less than this value (in seconds).
static constexpr base::TimeDelta kLocationChangeInterval =
    base::TimeDelta::FromSeconds(10);
}  // namespace

WebLayerRenderFrameObserver::WebLayerRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame), translate_agent_(nullptr) {
  // Don't do anything for subframes.
  if (!render_frame->IsMainFrame())
    return;

  // TODO(crbug.com/1073370): Handle case where subframe translation is enabled.
  DCHECK(!translate::IsSubFrameTranslationEnabled());
  translate_agent_ =
      new translate::TranslateAgent(render_frame, ISOLATED_WORLD_ID_TRANSLATE,
                                    /*extension_scheme=*/"");
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
      // We should run language detection only once. Parsing finishes before
      // the page loads, so let's pick that timing.
      CapturePageText();
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
void WebLayerRenderFrameObserver::CapturePageText() {
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

  // Don't index/capture pages that are being prerendered.
  if (prerender::PrerenderHelper::IsPrerendering(render_frame()))
    return;

  // Don't capture contents unless there is a translate agent to consume them.
  if (!translate_agent_)
    return;

  // Don't index/capture pages that are being prerendered.
  if (prerender::PrerenderHelper::IsPrerendering(render_frame()))
    return;

  base::TimeTicks capture_begin_time = base::TimeTicks::Now();

  // Retrieve the frame's full text (up to kMaxIndexChars), and pass it to the
  // translate helper for language detection and possible translation.
  // TODO(http://crbug.com/585164)): Update this when the corresponding usage of
  // this function in //chrome is updated.
  base::string16 contents =
      blink::WebFrameContentDumper::DeprecatedDumpFrameTreeAsText(
          frame, kMaxIndexChars)
          .Utf16();

  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeTicks::Now() - capture_begin_time);

  translate_agent_->PageCaptured(contents);
}

void WebLayerRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace weblayer
