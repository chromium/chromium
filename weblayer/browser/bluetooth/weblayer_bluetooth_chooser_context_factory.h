// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_CHOOSER_CONTEXT_FACTORY_H_
#define WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_CHOOSER_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace permissions {
class BluetoothChooserContext;
}

namespace weblayer {

class WebLayerBluetoothChooserContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static permissions::BluetoothChooserContext* GetForBrowserContext(
      content::BrowserContext* context);
  static permissions::BluetoothChooserContext* GetForBrowserContextIfExists(
      content::BrowserContext* context);
  static WebLayerBluetoothChooserContextFactory* GetInstance();

  WebLayerBluetoothChooserContextFactory(
      const WebLayerBluetoothChooserContextFactory&) = delete;
  WebLayerBluetoothChooserContextFactory& operator=(
      const WebLayerBluetoothChooserContextFactory&) = delete;

 private:
  friend base::NoDestructor<WebLayerBluetoothChooserContextFactory>;

  WebLayerBluetoothChooserContextFactory();
  ~WebLayerBluetoothChooserContextFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_CHOOSER_CONTEXT_FACTORY_H_
