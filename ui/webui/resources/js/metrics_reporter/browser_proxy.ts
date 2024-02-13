// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import type {PageMetricsHostRemote} from '../metrics_reporter.mojom-webui.js';
import {PageMetricsCallbackRouter, PageMetricsHost} from '../metrics_reporter.mojom-webui.js';

export interface BrowserProxy {
  getMark(name: string): Promise<{markedTime: TimeDelta | null}>;
  clearMark(name: string): void;
  umaReportTime(name: string, time: TimeDelta): void;
  getCallbackRouter(): PageMetricsCallbackRouter;
  now(): bigint;
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: PageMetricsCallbackRouter;
  host: PageMetricsHostRemote;

  constructor() {
    this.callbackRouter = new PageMetricsCallbackRouter();
    this.host = PageMetricsHost.getRemote();
    this.host.onPageRemoteCreated(
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  getMark(name: string) {
    return this.host.onGetMark(name);
  }

  clearMark(name: string) {
    this.host.onClearMark(name);
  }

  umaReportTime(name: string, time: TimeDelta) {
    this.host.onUmaReportTime(name, time);
  }

  now(): bigint {
    return chrome.timeTicks.nowInMicroseconds();
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

let instance: BrowserProxy|null = null;
