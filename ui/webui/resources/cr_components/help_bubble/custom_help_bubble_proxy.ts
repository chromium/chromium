// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CustomHelpBubbleHandlerInterface} from './custom_help_bubble.mojom-webui.js';
import {CustomHelpBubbleHandlerFactory, CustomHelpBubbleHandlerRemote} from './custom_help_bubble.mojom-webui.js';

export interface CustomHelpBubbleProxy {
  getHandler(): CustomHelpBubbleHandlerInterface;
}

export class CustomHelpBubbleProxyImpl implements CustomHelpBubbleProxy {
  private handler_ = new CustomHelpBubbleHandlerRemote();

  constructor() {
    const factory = CustomHelpBubbleHandlerFactory.getRemote();
    factory.createCustomHelpBubbleHandler(
        this.handler_.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): CustomHelpBubbleProxy {
    return instance || (instance = new CustomHelpBubbleProxyImpl());
  }

  static setInstance(obj: CustomHelpBubbleProxy) {
    instance = obj;
  }

  getHandler(): CustomHelpBubbleHandlerRemote {
    return this.handler_;
  }
}

let instance: CustomHelpBubbleProxy|null = null;
