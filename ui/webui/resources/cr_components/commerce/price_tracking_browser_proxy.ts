// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

import {PageCallbackRouter, PriceTrackingHandlerFactory, PriceTrackingHandlerRemote} from './price_tracking.mojom-webui.js';
import type {BookmarkProductInfo} from './shared.mojom-webui.js';

let instance: PriceTrackingBrowserProxy|null = null;

export interface PriceTrackingBrowserProxy {
  trackPriceForBookmark(bookmarkId: bigint): void;
  untrackPriceForBookmark(bookmarkId: bigint): void;
  setPriceTrackingStatusForCurrentUrl(track: boolean): void;
  getAllPriceTrackedBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  getAllShoppingBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  getParentBookmarkFolderNameForCurrentUrl(): Promise<{name: String16}>;
  getShoppingCollectionBookmarkFolderId(): Promise<{collectionId: bigint}>;
  showBookmarkEditorForCurrentUrl(): void;
  getCallbackRouter(): PageCallbackRouter;
}

export class PriceTrackingBrowserProxyImpl implements
    PriceTrackingBrowserProxy {
  handler: PriceTrackingHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new PriceTrackingHandlerRemote();

    const factory = PriceTrackingHandlerFactory.getRemote();
    factory.createPriceTrackingHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  trackPriceForBookmark(bookmarkId: bigint) {
    this.handler.trackPriceForBookmark(bookmarkId);
  }

  untrackPriceForBookmark(bookmarkId: bigint) {
    this.handler.untrackPriceForBookmark(bookmarkId);
  }

  setPriceTrackingStatusForCurrentUrl(track: boolean) {
    this.handler.setPriceTrackingStatusForCurrentUrl(track);
  }

  getAllPriceTrackedBookmarkProductInfo() {
    return this.handler.getAllPriceTrackedBookmarkProductInfo();
  }

  getAllShoppingBookmarkProductInfo() {
    return this.handler.getAllShoppingBookmarkProductInfo();
  }

  getShoppingCollectionBookmarkFolderId() {
    return this.handler.getShoppingCollectionBookmarkFolderId();
  }

  getParentBookmarkFolderNameForCurrentUrl() {
    return this.handler.getParentBookmarkFolderNameForCurrentUrl();
  }

  showBookmarkEditorForCurrentUrl() {
    this.handler.showBookmarkEditorForCurrentUrl();
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): PriceTrackingBrowserProxy {
    return instance || (instance = new PriceTrackingBrowserProxyImpl());
  }

  static setInstance(obj: PriceTrackingBrowserProxy) {
    instance = obj;
  }
}
