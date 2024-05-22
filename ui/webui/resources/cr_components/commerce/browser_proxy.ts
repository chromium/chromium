// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {BookmarkProductInfo, PriceInsightsInfo, ProductInfo, ProductSpecifications, ProductSpecificationsSet, UrlInfo} from './shopping_service.mojom-webui.js';
import {PageCallbackRouter, ShoppingServiceHandlerFactory, ShoppingServiceHandlerRemote} from './shopping_service.mojom-webui.js';

let instance: BrowserProxy|null = null;

export interface BrowserProxy {
  getAllPriceTrackedBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  getAllShoppingBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  trackPriceForBookmark(bookmarkId: bigint): void;
  untrackPriceForBookmark(bookmarkId: bigint): void;
  getProductInfoForCurrentUrl(): Promise<{productInfo: ProductInfo}>;
  getPriceInsightsInfoForCurrentUrl():
      Promise<{priceInsightsInfo: PriceInsightsInfo}>;
  showInsightsSidePanelUi(): void;
  getUrlInfosForOpenTabs(): Promise<{urlInfos: UrlInfo[]}>;
  getUrlInfosForRecentlyViewedTabs(): Promise<{urlInfos: UrlInfo[]}>;
  isShoppingListEligible(): Promise<{eligible: boolean}>;
  getShoppingCollectionBookmarkFolderId(): Promise<{collectionId: bigint}>;
  getPriceTrackingStatusForCurrentUrl(): Promise<{tracked: boolean}>;
  setPriceTrackingStatusForCurrentUrl(track: boolean): void;
  openUrlInNewTab(url: Url): void;
  switchToOrOpenTab(url: Url): void;
  getParentBookmarkFolderNameForCurrentUrl(): Promise<{name: String16}>;
  showBookmarkEditorForCurrentUrl(): void;
  showFeedback(): void;
  getCallbackRouter(): PageCallbackRouter;
  getProductInfoForUrl(url: Url): Promise<{productInfo: ProductInfo}>;
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
}

export class BrowserProxyImpl implements BrowserProxy {
  handler: ShoppingServiceHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new ShoppingServiceHandlerRemote();

    const factory = ShoppingServiceHandlerFactory.getRemote();
    factory.createShoppingServiceHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getAllPriceTrackedBookmarkProductInfo() {
    return this.handler.getAllPriceTrackedBookmarkProductInfo();
  }

  getAllShoppingBookmarkProductInfo() {
    return this.handler.getAllShoppingBookmarkProductInfo();
  }

  trackPriceForBookmark(bookmarkId: bigint) {
    this.handler.trackPriceForBookmark(bookmarkId);
  }

  untrackPriceForBookmark(bookmarkId: bigint) {
    this.handler.untrackPriceForBookmark(bookmarkId);
  }

  getProductInfoForCurrentUrl() {
    return this.handler.getProductInfoForCurrentUrl();
  }

  getProductInfoForUrl(url: Url) {
    return this.handler.getProductInfoForUrl(url);
  }

  getPriceInsightsInfoForCurrentUrl() {
    return this.handler.getPriceInsightsInfoForCurrentUrl();
  }

  getProductSpecificationsForUrls(urls: Url[]) {
    return this.handler.getProductSpecificationsForUrls(urls);
  }

  getUrlInfosForOpenTabs() {
    return this.handler.getUrlInfosForOpenTabs();
  }

  getUrlInfosForRecentlyViewedTabs() {
    return this.handler.getUrlInfosForRecentlyViewedTabs();
  }

  showInsightsSidePanelUi() {
    this.handler.showInsightsSidePanelUI();
  }

  isShoppingListEligible() {
    return this.handler.isShoppingListEligible();
  }

  getShoppingCollectionBookmarkFolderId() {
    return this.handler.getShoppingCollectionBookmarkFolderId();
  }

  getPriceTrackingStatusForCurrentUrl() {
    return this.handler.getPriceTrackingStatusForCurrentUrl();
  }

  setPriceTrackingStatusForCurrentUrl(track: boolean) {
    this.handler.setPriceTrackingStatusForCurrentUrl(track);
  }

  openUrlInNewTab(url: Url) {
    this.handler.openUrlInNewTab(url);
  }

  switchToOrOpenTab(url: Url) {
    this.handler.switchToOrOpenTab(url);
  }

  getParentBookmarkFolderNameForCurrentUrl() {
    return this.handler.getParentBookmarkFolderNameForCurrentUrl();
  }

  showBookmarkEditorForCurrentUrl() {
    this.handler.showBookmarkEditorForCurrentUrl();
  }

  showFeedback() {
    this.handler.showFeedback();
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

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}
