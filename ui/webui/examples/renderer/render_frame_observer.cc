// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/renderer/render_frame_observer.h"

#include "base/check.h"
#include "base/logging.h"
#include "components/guest_contents/renderer/swap_render_frame.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace webui_examples {

namespace {

class RenderFrameStatus final : public content::RenderFrameObserver {
 public:
  explicit RenderFrameStatus(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame) {}
  ~RenderFrameStatus() final = default;

  bool IsRenderFrameAvailable() { return render_frame() != nullptr; }

  // RenderFrameObserver implementation.
  void OnDestruct() final {}
};

void AllowCustomElementNameRegistration(v8::Local<v8::Function> callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  blink::WebCustomElement::EmbedderNamesAllowedScope embedder_names_scope;
  callback->Call(context, context->Global(), 0, nullptr).ToLocalChecked();
}

content::RenderFrame* GetRenderFrame(v8::Local<v8::Value> value) {
  v8::Local<v8::Context> context;
  if (!v8::Local<v8::Object>::Cast(value)->GetCreationContext().ToLocal(
          &context)) {
    if (context.IsEmpty()) {
      return nullptr;
    }
  }
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForContext(context);
  if (!frame) {
    return nullptr;
  }
  return content::RenderFrame::FromWebFrame(frame);
}

void AttachIframeGuest(int guest_contents_id,
                       v8::Local<v8::Object> content_window) {
  // attachIframeGuest(guestInstanceId, contentWindow)
  // Getting the attach params could destroy the frame while it executes JS,
  // so observe the render frame for destruction. We don't expect for this to
  // occur in the Webshell, so we CHECK against it.
  content::RenderFrame* render_frame = GetRenderFrame(content_window);
  RenderFrameStatus render_frame_status(render_frame);
  CHECK(render_frame_status.IsRenderFrameAvailable());

  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  // Parent must exist.
  blink::WebFrame* parent_frame = frame->Parent();
  CHECK(parent_frame);
  CHECK(parent_frame->IsWebLocalFrame());

  guest_contents::renderer::SwapRenderFrame(render_frame, guest_contents_id);
}

}  // namespace

RenderFrameObserver::RenderFrameObserver(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

RenderFrameObserver::~RenderFrameObserver() = default;

void RenderFrameObserver::SelfOwn(
    std::unique_ptr<RenderFrameObserver> this_instance) {
  DCHECK_EQ(this, this_instance.get());
  this_instance_ = std::move(this_instance);
}

void RenderFrameObserver::OnDestruct() {
  this_instance_.reset();
}

void RenderFrameObserver::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
  if (!url.SchemeIs("chrome") || url.GetHost() != "browser") {
    this_instance_.reset();
    return;
  }
}

void RenderFrameObserver::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::ObjectTemplate> webshell_template =
      gin::ObjectTemplateBuilder(isolate)
          .SetMethod("allowWebviewElementRegistration",
                     &AllowCustomElementNameRegistration)
          .SetMethod("attachIframeGuest", &AttachIframeGuest)
          .Build();

  context->Global()
      ->CreateDataProperty(
          context, gin::StringToV8(isolate, "webshell"),
          webshell_template->NewInstance(context).ToLocalChecked())
      .FromJust();
}

}  // namespace webui_examples
