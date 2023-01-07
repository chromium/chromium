// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/weblayer_render_thread_observer.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace weblayer {

WebLayerRenderThreadObserver::WebLayerRenderThreadObserver() = default;

WebLayerRenderThreadObserver::~WebLayerRenderThreadObserver() = default;

void WebLayerRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface<
      mojom::RendererConfiguration>(base::BindRepeating(
      &WebLayerRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void WebLayerRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(mojom::RendererConfiguration::Name_);
}

// weblayer::mojom::RendererConfiguration:
void WebLayerRenderThreadObserver::SetInitialConfiguration(
    mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
        content_settings_manager) {
  if (content_settings_manager)
    content_settings_manager_.Bind(std::move(content_settings_manager));
}

void WebLayerRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::RendererConfiguration> receiver) {
  renderer_configuration_receivers_.Add(this, std::move(receiver));
}

}  // namespace weblayer
