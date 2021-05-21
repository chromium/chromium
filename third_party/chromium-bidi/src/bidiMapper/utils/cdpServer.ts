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
import { IServer } from './iServer';
import { log } from './log';
const logCdp = log('cdp');

/**
 * An abstruction providing access to CDP.
 */
export class CdpBinding {
  private _messageHandlerSetter: (messageHandler: (string) => void) => void;
  private _sendMessage: (string) => void;

  static runCdpServer(
    sendMessage: (cdpMessage: string) => void,
    messageHandlerSetter: (messageHandler: (cdpMessage: string) => void) => void
  ): CdpServer {
    const cdpBinding = new CdpBinding(sendMessage, messageHandlerSetter);
    return new CdpServer(cdpBinding);
  }

  constructor(
    sendMessage: (cdpMessage: string) => void,
    messageHandlerSetter: (messageHandler: (cdpMessage: string) => void) => void
  ) {
    this._messageHandlerSetter = messageHandlerSetter;
    this._sendMessage = sendMessage;
  }

  public set onmessage(messageHandler: (string) => void) {
    this._messageHandlerSetter(messageHandler);
  }

  public sendMessage(message: string) {
    this._sendMessage(message);
  }
}

export class CdpServer implements IServer {
  private _cdpBinding: CdpBinding;
  private _commandCallbacks: Map<number, (cdpMessageObj: any) => void> =
    new Map();
  private _handlers: ((cdMessageObj: any) => void)[] = new Array();

  constructor(cdpBinding: CdpBinding) {
    this._cdpBinding = cdpBinding;
    this._cdpBinding.onmessage = (cdpMessageStr: string) => {
      this._onCdpMessage(cdpMessageStr);
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

  private _onCdpMessage(cdpMessageStr: string): void {
    logCdp('received < ' + cdpMessageStr);

    const cdpMessageObj = JSON.parse(cdpMessageStr);
    if (this._commandCallbacks.has(cdpMessageObj.id)) {
      this._commandCallbacks.get(cdpMessageObj.id)(cdpMessageObj.result);
      return;
    } else {
      for (let handler of this._handlers) handler(cdpMessageObj);
    }
  }
}
