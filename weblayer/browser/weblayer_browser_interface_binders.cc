// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_browser_interface_binders.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/browser/prerender_contents.h"
#include "components/no_state_prefetch/browser/prerender_processor_impl.h"
#include "components/no_state_prefetch/common/prerender_canceler.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "weblayer/browser/no_state_prefetch/prerender_processor_impl_delegate_impl.h"
#include "weblayer/browser/no_state_prefetch/prerender_utils.h"
#include "weblayer/browser/translate_client_impl.h"
#include "weblayer/browser/webui/weblayer_internals.mojom.h"
#include "weblayer/browser/webui/weblayer_internals_ui.h"

#if defined(OS_ANDROID)
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
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

void BindPrerenderProcessor(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::PrerenderProcessor> receiver) {
  prerender::PrerenderProcessorImpl::Create(
      frame_host, std::move(receiver),
      std::make_unique<PrerenderProcessorImplDelegateImpl>());
}

void BindPrerenderCanceler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<prerender::mojom::PrerenderCanceler> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* prerender_contents = PrerenderContentsFromWebContents(web_contents);
  if (!prerender_contents)
    return;
  prerender_contents->AddPrerenderCancelerReceiver(std::move(receiver));
}

#if defined(OS_ANDROID)
// TODO(https://crbug.com/1037884): Remove this.
class StubInstalledAppProvider : public blink::mojom::InstalledAppProvider {
 public:
  StubInstalledAppProvider() {}
  ~StubInstalledAppProvider() override = default;

  // InstalledAppProvider overrides:
  void FilterInstalledApps(
      std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
      const GURL& manifest_url,
      FilterInstalledAppsCallback callback) override {
    std::move(callback).Run(std::vector<blink::mojom::RelatedApplicationPtr>());
  }

  static void Create(
      content::RenderFrameHost* rfh,
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<StubInstalledAppProvider>(),
                                std::move(receiver));
  }
};

template <typename Interface>
void ForwardToJavaWebContents(content::RenderFrameHost* frame_host,
                              mojo::PendingReceiver<Interface> receiver) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  if (contents)
    contents->GetJavaInterfaces()->GetInterface(std::move(receiver));
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

  // When Prerender2 is enabled, the content layer already added a binder.
  if (!base::FeatureList::IsEnabled(blink::features::kPrerender2)) {
    map->Add<blink::mojom::PrerenderProcessor>(
        base::BindRepeating(&BindPrerenderProcessor));
  }
  map->Add<prerender::mojom::PrerenderCanceler>(
      base::BindRepeating(&BindPrerenderCanceler));

#if defined(OS_ANDROID)
  // TODO(https://crbug.com/1037884): Remove this.
  map->Add<blink::mojom::InstalledAppProvider>(
      base::BindRepeating(&StubInstalledAppProvider::Create));
  map->Add<blink::mojom::ShareService>(base::BindRepeating(
      &ForwardToJavaWebContents<blink::mojom::ShareService>));
#endif
}

}  // namespace weblayer
