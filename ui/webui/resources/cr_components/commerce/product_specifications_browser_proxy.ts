// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {PageCallbackRouter, ProductSpecificationsHandlerFactory, ProductSpecificationsHandlerRemote} from './product_specifications.mojom-webui.js';
import type {DisclosureVersion} from './product_specifications.mojom-webui.ts';

let instance: ProductSpecificationsBrowserProxy|null = null;

export interface ProductSpecificationsBrowserProxy {
  getCallbackRouter(): PageCallbackRouter;
  showProductSpecificationsSetForUuid(uuid: Uuid, inNewTab: boolean): void;
  setAcceptedDisclosureVersion(version: DisclosureVersion): void;
  maybeShowDisclosure(urls: Url[], name: string, setId: string):
      Promise<{disclosureShown: boolean}>;
  declineDisclosure(): void;
  showSyncSetupFlow(): void;
  getPageTitleFromHistory(url: Url): Promise<{title: string}>;
}

export class ProductSpecificationsBrowserProxyImpl implements
    ProductSpecificationsBrowserProxy {
  handler: ProductSpecificationsHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new ProductSpecificationsHandlerRemote();

    const factory = ProductSpecificationsHandlerFactory.getRemote();
    factory.createProductSpecificationsHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  showSyncSetupFlow() {
    this.handler.showSyncSetupFlow();
  }

  setAcceptedDisclosureVersion(version: DisclosureVersion) {
    this.handler.setAcceptedDisclosureVersion(version);
  }

  showProductSpecificationsSetForUuid(uuid: Uuid, inNewTab: boolean) {
    this.handler.showProductSpecificationsSetForUuid(uuid, inNewTab);
  }

  maybeShowDisclosure(urls: Url[], name: string, setId: string) {
    return this.handler.maybeShowDisclosure(urls, name, setId);
  }

  declineDisclosure() {
    this.handler.declineDisclosure();
  }

  getPageTitleFromHistory(url: Url): Promise<{title: string}> {
    return this.handler.getPageTitleFromHistory(url);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): ProductSpecificationsBrowserProxy {
    return instance || (instance = new ProductSpecificationsBrowserProxyImpl());
  }

  static setInstance(obj: ProductSpecificationsBrowserProxy) {
    instance = obj;
  }
}
