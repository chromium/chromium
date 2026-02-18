// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ProductInfo} from './shared.mojom-webui.js';
import type {PriceInsightsInfo, UrlInfo} from './shopping_service.mojom-webui.js';
import {ShoppingServiceHandlerFactory, ShoppingServiceHandlerRemote} from './shopping_service.mojom-webui.js';

let instance: ShoppingServiceBrowserProxy|null = null;

export interface ShoppingServiceBrowserProxy {
  getProductInfoForCurrentUrl(): Promise<{productInfo: ProductInfo}>;
  getPriceInsightsInfoForCurrentUrl():
      Promise<{priceInsightsInfo: PriceInsightsInfo}>;
  getUrlInfosForProductTabs(): Promise<{urlInfos: UrlInfo[]}>;
  getUrlInfosForRecentlyViewedTabs(): Promise<{urlInfos: UrlInfo[]}>;
  isShoppingListEligible(): Promise<{eligible: boolean}>;
  getPriceTrackingStatusForCurrentUrl(): Promise<{tracked: boolean}>;
  openUrlInNewTab(url: Url): void;
  switchToOrOpenTab(url: Url): void;
  getPriceInsightsInfoForUrl(url: Url):
      Promise<{priceInsightsInfo: PriceInsightsInfo}>;
  getProductInfoForUrl(url: Url): Promise<{productInfo: ProductInfo}>;
  getProductInfoForUrls(urls: Url[]): Promise<{productInfos: ProductInfo[]}>;
}

export class ShoppingServiceBrowserProxyImpl implements
    ShoppingServiceBrowserProxy {
  handler: ShoppingServiceHandlerRemote;

  constructor() {
    this.handler = new ShoppingServiceHandlerRemote();

    const factory = ShoppingServiceHandlerFactory.getRemote();
    factory.createShoppingServiceHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getProductInfoForCurrentUrl() {
    return this.handler.getProductInfoForCurrentUrl();
  }

  getProductInfoForUrl(url: Url) {
    return this.handler.getProductInfoForUrl(url);
  }

  getProductInfoForUrls(urls: Url[]) {
    return this.handler.getProductInfoForUrls(urls);
  }

  getPriceInsightsInfoForCurrentUrl() {
    return this.handler.getPriceInsightsInfoForCurrentUrl();
  }

  getPriceInsightsInfoForUrl(url: Url) {
    return this.handler.getPriceInsightsInfoForUrl(url);
  }

  getUrlInfosForProductTabs() {
    return this.handler.getUrlInfosForProductTabs();
  }

  getUrlInfosForRecentlyViewedTabs() {
    return this.handler.getUrlInfosForRecentlyViewedTabs();
  }

  isShoppingListEligible() {
    return this.handler.isShoppingListEligible();
  }

  getPriceTrackingStatusForCurrentUrl() {
    return this.handler.getPriceTrackingStatusForCurrentUrl();
  }

  openUrlInNewTab(url: Url) {
    this.handler.openUrlInNewTab(url);
  }

  switchToOrOpenTab(url: Url) {
    this.handler.switchToOrOpenTab(url);
  }

  static getInstance(): ShoppingServiceBrowserProxy {
    return instance || (instance = new ShoppingServiceBrowserProxyImpl());
  }

  static setInstance(obj: ShoppingServiceBrowserProxy) {
    instance = obj;
  }
}
