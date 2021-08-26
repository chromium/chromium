// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommandHandlerFactory, CommandHandlerRemote} from './browser_command.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending the browser commands to the browser and
 * receiving the browser response.
 */

/** @type {BrowserCommandProxy} */
let instance = null;

export class BrowserCommandProxy {
  /** @return {!BrowserCommandProxy} */
  static getInstance() {
    return instance || (instance = new BrowserCommandProxy());
  }

  /** @param {BrowserCommandProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @type {!CommandHandlerRemote} */
    this.handler = new CommandHandlerRemote();
    const factory = CommandHandlerFactory.getRemote();
    factory.createBrowserCommandHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}
