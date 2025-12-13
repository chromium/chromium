// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TrackedElementHandlerInterface, TrackedElementHandlerRemote} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {TrackedElementHandler} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';

export interface TrackedElementProxy {
  getHandler(): TrackedElementHandlerInterface;
}

/**
 * Proxy for the TrackedElementHandler mojo interface.
 */
export class TrackedElementProxyImpl implements TrackedElementProxy {
  private handler_: TrackedElementHandlerRemote =
      TrackedElementHandler.getRemote();

  getHandler(): TrackedElementHandlerInterface {
    return this.handler_;
  }

  static getInstance(): TrackedElementProxy {
    return instance || (instance = new TrackedElementProxyImpl());
  }

  static setInstance(obj: TrackedElementProxy) {
    instance = obj;
  }
}

let instance: TrackedElementProxy|null = null;
