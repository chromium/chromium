/**
 * Copyright 2021 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
import { ServerBinding, IServer } from './iServer';
import { log } from './log';
const logCdp = log('cdp');

export class CdpServer implements IServer {
  private _cdpBinding: ServerBinding;
  private _commandCallbacks: Map<number, (messageObj: any) => void> = new Map();
  private _handlers: ((messageObj: any) => void)[] = new Array();

  constructor(cdpBinding: ServerBinding) {
    this._cdpBinding = cdpBinding;
    this._cdpBinding.onmessage = (messageStr: string) => {
      this._onCdpMessage(messageStr);
    };
  }

  /**
   * Sets handler, which will be called for each CDP message, except commands.
   * Commands result will be returned by the command promise.
   * @param handler
   */
  setOnMessage(handler: (messageObj: any) => Promise<void>): void {
    this._handlers.push(handler);
  }

  /**
   * Returns command promise, which will be resolved wth the command result after receiving CDP result.
   * @param messageObj Message object to be sent. Will be automatically enriched with `id`.
   */
  sendMessage(messageObj: any): Promise<any> {
    return new Promise((resolve) => {
      const id = this._commandCallbacks.size;
      this._commandCallbacks.set(id, resolve);
      messageObj.id = id;
      const messageStr = JSON.stringify(messageObj);

      logCdp('sent > ' + messageStr);
      this._cdpBinding.sendMessage(messageStr);
    });
  }

  private _onCdpMessage(messageStr: string): void {
    logCdp('received < ' + messageStr);

    const messageObj = JSON.parse(messageStr);
    if (this._commandCallbacks.has(messageObj.id)) {
      this._commandCallbacks.get(messageObj.id)(messageObj.result);
      return;
    } else {
      for (let handler of this._handlers) handler(messageObj);
    }
  }
}
