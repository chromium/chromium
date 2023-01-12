// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_browser_interface_binders.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl.h"
#include "components/no_state_prefetch/common/prerender_canceler.mojom.h"
#include "components/payments/content/payment_credential_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_processor_impl_delegate_impl.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_utils.h"
#include "weblayer/browser/translate_client_impl.h"
#include "weblayer/browser/webui/weblayer_internals.mojom.h"
#include "weblayer/browser/webui/weblayer_internals_ui.h"

#if BUILDFLAG(IS_ANDROID)
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif

namespace weblayer {
namespace {

void BindContentTranslateDriver(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver) {
  // Translation does not currently work in subframes.
  // TODO(crbug.com/1073370): Transition WebLayer to per-frame translation
  // architecture once it's ready.
  if (host->GetParent())
    return;

  auto* contents = content::WebContents::FromRenderFrameHost(host);
  if (!contents)
    return;

  TranslateClientImpl* const translate_client =
      TranslateClientImpl::FromWebContents(contents);
  translate_client->translate_driver()->AddReceiver(std::move(receiver));
}

void BindPageHandler(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<weblayer_internals::mojom::PageHandler> receiver) {
  auto* contents = content::WebContents::FromRenderFrameHost(host);
  if (!contents)
    return;

  content::WebUI* web_ui = contents->GetWebUI();

  // Performs a safe downcast to the concrete WebUIController subclass.
  WebLayerInternalsUI* concrete_controller =
      web_ui ? web_ui->GetController()->GetAs<WebLayerInternalsUI>() : nullptr;

  // This is expected to be called only for main frames and for the right
  // WebUI pages matching the same WebUI associated to the RenderFrameHost.
  if (host->GetParent() || !concrete_controller)
    return;

  concrete_controller->BindInterface(std::move(receiver));
}

void BindNoStatePrefetchProcessor(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::NoStatePrefetchProcessor> receiver) {
  prerender::NoStatePrefetchProcessorImpl::Create(
      frame_host, std::move(receiver),
      std::make_unique<NoStatePrefetchProcessorImplDelegateImpl>());
}

void BindPrerenderCanceler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<prerender::mojom::PrerenderCanceler> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* no_state_prefetch_contents =
      NoStatePrefetchContentsFromWebContents(web_contents);
  if (!no_state_prefetch_contents)
    return;
  no_state_prefetch_contents->AddPrerenderCancelerReceiver(std::move(receiver));
}

#if BUILDFLAG(IS_ANDROID)
template <typename Interface>
void ForwardToJavaWebContents(content::RenderFrameHost* frame_host,
                              mojo::PendingReceiver<Interface> receiver) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  if (contents)
    contents->GetJavaInterfaces()->GetInterface(std::move(receiver));
}

template <typename Interface>
void ForwardToJavaFrame(content::RenderFrameHost* render_frame_host,
                        mojo::PendingReceiver<Interface> receiver) {
  render_frame_host->GetJavaInterfaces()->GetInterface(std::move(receiver));
}
#endif

}  // namespace

void PopulateWebLayerFrameBinders(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<weblayer_internals::mojom::PageHandler>(
      base::BindRepeating(&BindPageHandler));

  map->Add<translate::mojom::ContentTranslateDriver>(
      base::BindRepeating(&BindContentTranslateDriver));

  map->Add<blink::mojom::NoStatePrefetchProcessor>(
      base::BindRepeating(&BindNoStatePrefetchProcessor));

  map->Add<prerender::mojom::PrerenderCanceler>(
      base::BindRepeating(&BindPrerenderCanceler));

#if BUILDFLAG(IS_ANDROID)
  map->Add<blink::mojom::InstalledAppProvider>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::InstalledAppProvider>));
  map->Add<blink::mojom::ShareService>(base::BindRepeating(
      &ForwardToJavaWebContents<blink::mojom::ShareService>));
  map->Add<payments::mojom::PaymentRequest>(base::BindRepeating(
      &ForwardToJavaFrame<payments::mojom::PaymentRequest>));
  map->Add<payments::mojom::PaymentCredential>(
      base::BindRepeating(&payments::CreatePaymentCredential));
#endif
}

}  // namespace weblayer
