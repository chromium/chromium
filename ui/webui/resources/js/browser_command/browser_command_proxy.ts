// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommandHandlerFactory, CommandHandlerRemote} from '../browser_command.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending the browser commands to the browser and
 * receiving the browser response.
 */

let instance: BrowserCommandProxy|null = null;

export class BrowserCommandProxy {
  static getInstance(): BrowserCommandProxy {
    return instance || (instance = new BrowserCommandProxy());
  }

  static setInstance(newInstance: BrowserCommandProxy) {
    instance = newInstance;
  }

  handler: CommandHandlerRemote;

  constructor() {
    this.handler = new CommandHandlerRemote();
    const factory = CommandHandlerFactory.getRemote();
    factory.createBrowserCommandHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}
