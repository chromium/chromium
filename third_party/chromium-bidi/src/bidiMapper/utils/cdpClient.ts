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
import { ServerBinding, AbstractServer } from './iServer';
import { log } from './log';
const logCdp = log('cdp');

// TODO: Remove. This is a quick test to verify we can import a node module.
import { version } from 'devtools-protocol/json/browser_protocol.json';
logCdp(`Using CDP version: ${version}`);

export class CdpClient extends AbstractServer {
  private _commandCallbacks: Map<number, (messageObj: any) => void> = new Map();

  constructor(cdpBinding: ServerBinding) {
    super(cdpBinding);

    this._binding.onmessage = (messageStr: string) => {
      this._onCdpMessage(messageStr);
    };
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
      this._binding.sendMessage(messageStr);
    });
  }

  private _onCdpMessage(messageStr: string): void {
    logCdp('received < ' + messageStr);

    const messageObj = JSON.parse(messageStr);
    if (this._commandCallbacks.has(messageObj.id)) {
      this._commandCallbacks.get(messageObj.id)(messageObj.result);
      return;
    } else {
      this.notifySubscribersOnMessage(messageObj);
    }
  }
}
