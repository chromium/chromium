// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {TrackedElementHandlerInterface} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {TrackedElementHandlerRemote} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';

import type {HelpBubbleHandlerInterface} from './help_bubble.mojom-webui.js';
import {HelpBubbleClientCallbackRouter, HelpBubbleHandlerRemote, PdfHelpBubbleHandlerFactory} from './help_bubble.mojom-webui.js';

export interface PdfHelpBubbleProxy {
  getTrackedElementHandler(): TrackedElementHandlerInterface;
  getHandler(): HelpBubbleHandlerInterface;
  getCallbackRouter(): HelpBubbleClientCallbackRouter;
}

export class PdfHelpBubbleProxyImpl implements PdfHelpBubbleProxy {
  private trackedElementHandler_ = new TrackedElementHandlerRemote();
  private callbackRouter_ = new HelpBubbleClientCallbackRouter();
  private handler_ = new HelpBubbleHandlerRemote();

  constructor(connectToMojo: boolean) {
    if (connectToMojo) {
      const factory = PdfHelpBubbleHandlerFactory.getRemote();
      factory.createHelpBubbleHandler(
          this.callbackRouter_.$.bindNewPipeAndPassRemote(),
          this.handler_.$.bindNewPipeAndPassReceiver());
      this.handler_.bindTrackedElementHandler(
          this.trackedElementHandler_.$.bindNewPipeAndPassReceiver());
    }
  }

  static getInstance(): PdfHelpBubbleProxy {
    return instance || (instance = new PdfHelpBubbleProxyImpl(false));
  }

  static createConnectedInstance() {
    assert(!instance);
    instance = new PdfHelpBubbleProxyImpl(true);
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

let instance: PdfHelpBubbleProxy|null = null;
