// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TrackedElementHandlerInterface} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {TrackedElementHandlerRemote} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';

import type {HelpBubbleHandlerInterface} from './help_bubble.mojom-webui.js';
import {HelpBubbleClientCallbackRouter, HelpBubbleHandlerFactory, HelpBubbleHandlerRemote} from './help_bubble.mojom-webui.js';

export interface HelpBubbleProxy {
  getTrackedElementHandler(): TrackedElementHandlerInterface;
  getHandler(): HelpBubbleHandlerInterface;
  getCallbackRouter(): HelpBubbleClientCallbackRouter;
}

export class HelpBubbleProxyImpl implements HelpBubbleProxy {
  private trackedElementHandler_ = new TrackedElementHandlerRemote();
  private callbackRouter_ = new HelpBubbleClientCallbackRouter();
  private handler_ = new HelpBubbleHandlerRemote();

  constructor() {
    const factory = HelpBubbleHandlerFactory.getRemote();
    factory.createHelpBubbleHandler(
        this.callbackRouter_.$.bindNewPipeAndPassRemote(),
        this.handler_.$.bindNewPipeAndPassReceiver());
    this.handler_.bindTrackedElementHandler(
        this.trackedElementHandler_.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): HelpBubbleProxy {
    return instance || (instance = new HelpBubbleProxyImpl());
  }

  static setInstance(obj: HelpBubbleProxy) {
    instance = obj;
  }

  getTrackedElementHandler(): TrackedElementHandlerRemote {
    return this.trackedElementHandler_;
  }

  getHandler(): HelpBubbleHandlerRemote {
    return this.handler_;
  }

  getCallbackRouter(): HelpBubbleClientCallbackRouter {
    return this.callbackRouter_;
  }
}

let instance: HelpBubbleProxy|null = null;