// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ProductInfo, ProductSpecificationsSet} from './shared.mojom-webui.js';
import type {PriceInsightsInfo, ProductSpecifications, ProductSpecificationsFeatureState, UrlInfo, UserFeedback} from './shopping_service.mojom-webui.js';
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
  getProductSpecificationsForUrls(urls: Url[]):
      Promise<{productSpecs: ProductSpecifications}>;
  getAllProductSpecificationsSets():
      Promise<{sets: ProductSpecificationsSet[]}>;
  getProductSpecificationsSetByUuid(uuid: Uuid):
      Promise<{set: ProductSpecificationsSet | null}>;
  addProductSpecificationsSet(name: string, urls: Url[]):
      Promise<{createdSet: ProductSpecificationsSet | null}>;
  deleteProductSpecificationsSet(uuid: Uuid): void;
  setNameForProductSpecificationsSet(uuid: Uuid, name: string):
      Promise<{updatedSet: ProductSpecificationsSet | null}>;
  setUrlsForProductSpecificationsSet(uuid: Uuid, urls: Url[]):
      Promise<{updatedSet: ProductSpecificationsSet | null}>;
  setProductSpecificationsUserFeedback(feedback: UserFeedback): void;
  getProductSpecificationsFeatureState():
      Promise<{state: ProductSpecificationsFeatureState | null}>;
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

  getProductSpecificationsForUrls(urls: Url[]) {
    return this.handler.getProductSpecificationsForUrls(urls);
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

  getAllProductSpecificationsSets() {
    return this.handler.getAllProductSpecificationsSets();
  }

  getProductSpecificationsSetByUuid(uuid: Uuid) {
    return this.handler.getProductSpecificationsSetByUuid(uuid);
  }

  addProductSpecificationsSet(name: string, urls: Url[]) {
    return this.handler.addProductSpecificationsSet(name, urls);
  }

  deleteProductSpecificationsSet(uuid: Uuid) {
    this.handler.deleteProductSpecificationsSet(uuid);
  }

  setNameForProductSpecificationsSet(uuid: Uuid, name: string) {
    return this.handler.setNameForProductSpecificationsSet(uuid, name);
  }

  setUrlsForProductSpecificationsSet(uuid: Uuid, urls: Url[]) {
    return this.handler.setUrlsForProductSpecificationsSet(uuid, urls);
  }

  setProductSpecificationsUserFeedback(feedback: UserFeedback) {
    this.handler.setProductSpecificationsUserFeedback(feedback);
  }

  getProductSpecificationsFeatureState() {
    return this.handler.getProductSpecificationsFeatureState();
  }

  static getInstance(): ShoppingServiceBrowserProxy {
    return instance || (instance = new ShoppingServiceBrowserProxyImpl());
  }

  static setInstance(obj: ShoppingServiceBrowserProxy) {
    instance = obj;
  }
}
