// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {TrackedElementProxyImpl} from '//resources/js/tracked_element/tracked_element_proxy.js';
import type {TrackedElementProxy} from '//resources/js/tracked_element/tracked_element_proxy.js';
import type {TrackedElementHandlerInterface} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {TrackedElementHandlerRemote, TrackedElementManagerCallbackRouter} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';

import type {HelpBubbleHandlerInterface} from './help_bubble.mojom-webui.js';
import {HelpBubbleClientCallbackRouter, HelpBubbleHandlerRemote, PdfHelpBubbleHandlerFactory} from './help_bubble.mojom-webui.js';

export interface PdfHelpBubbleProxy {
  getHandler(): HelpBubbleHandlerInterface;
  getCallbackRouter(): HelpBubbleClientCallbackRouter;
}

class PdfTrackedElementProxyImpl implements TrackedElementProxy {
  private handler_: TrackedElementHandlerInterface;
  callbackRouter: TrackedElementManagerCallbackRouter =
      new TrackedElementManagerCallbackRouter();

  constructor(handler: TrackedElementHandlerInterface) {
    this.handler_ = handler;
  }

  getHandler(): TrackedElementHandlerInterface {
    return this.handler_;
  }
}

export class PdfHelpBubbleProxyImpl implements PdfHelpBubbleProxy {
  private trackedElementHandler_ = new TrackedElementHandlerRemote();
  private callbackRouter_ = new HelpBubbleClientCallbackRouter();
  private helpBubbleHandler_ = new HelpBubbleHandlerRemote();

  constructor(connectToMojo: boolean) {
    if (connectToMojo) {
      const factory = PdfHelpBubbleHandlerFactory.getRemote();
      factory.createHelpBubbleHandler(
          this.callbackRouter_.$.bindNewPipeAndPassRemote(),
          this.helpBubbleHandler_.$.bindNewPipeAndPassReceiver(),
          this.trackedElementHandler_.$.bindNewPipeAndPassReceiver());
      TrackedElementProxyImpl.setInstance(
          new PdfTrackedElementProxyImpl(this.trackedElementHandler_));
    }
  }

  static getInstance(): PdfHelpBubbleProxy {
    return instance || (instance = new PdfHelpBubbleProxyImpl(false));
  }

  static createConnectedInstance() {
    assert(!instance);
    instance = new PdfHelpBubbleProxyImpl(true);
  }

  getHandler(): HelpBubbleHandlerRemote {
    return this.helpBubbleHandler_;
  }

  getCallbackRouter(): HelpBubbleClientCallbackRouter {
    return this.callbackRouter_;
  }
}

let instance: PdfHelpBubbleProxy|null = null;
