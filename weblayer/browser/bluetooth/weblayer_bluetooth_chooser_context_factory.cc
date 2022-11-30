// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/bluetooth/weblayer_bluetooth_chooser_context_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
WebLayerBluetoothChooserContextFactory*
WebLayerBluetoothChooserContextFactory::GetInstance() {
  static base::NoDestructor<WebLayerBluetoothChooserContextFactory> factory;
  return factory.get();
}

// static
permissions::BluetoothChooserContext*
WebLayerBluetoothChooserContextFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<permissions::BluetoothChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
permissions::BluetoothChooserContext*
WebLayerBluetoothChooserContextFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<permissions::BluetoothChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

WebLayerBluetoothChooserContextFactory::WebLayerBluetoothChooserContextFactory()
    : BrowserContextKeyedServiceFactory(
          "BluetoothChooserContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

WebLayerBluetoothChooserContextFactory::
    ~WebLayerBluetoothChooserContextFactory() = default;

KeyedService* WebLayerBluetoothChooserContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::BluetoothChooserContext(context);
}

content::BrowserContext*
WebLayerBluetoothChooserContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

void WebLayerBluetoothChooserContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* bluetooth_chooser_context = GetForBrowserContextIfExists(context);
  if (bluetooth_chooser_context)
    bluetooth_chooser_context->FlushScheduledSaveSettingsCalls();
}

}  // namespace weblayer
