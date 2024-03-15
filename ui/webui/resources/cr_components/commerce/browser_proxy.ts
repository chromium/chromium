// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import type {BookmarkProductInfo, PriceInsightsInfo, ProductInfo} from './shopping_service.mojom-webui.js';
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
  isShoppingListEligible(): Promise<{eligible: boolean}>;
  getShoppingCollectionBookmarkFolderId(): Promise<{collectionId: bigint}>;
  getPriceTrackingStatusForCurrentUrl(): Promise<{tracked: boolean}>;
  setPriceTrackingStatusForCurrentUrl(track: boolean): void;
  openUrlInNewTab(url: Url): void;
  getParentBookmarkFolderNameForCurrentUrl(): Promise<{name: String16}>;
  showBookmarkEditorForCurrentUrl(): void;
  showFeedback(): void;
  getCallbackRouter(): PageCallbackRouter;
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

  getParentBookmarkFolderNameForCurrentUrl() {
    return this.handler.getParentBookmarkFolderNameForCurrentUrl();
  }

  showBookmarkEditorForCurrentUrl() {
    this.handler.showBookmarkEditorForCurrentUrl();
  }

  showFeedback() {
    this.handler.showFeedback();
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
