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

/**
 * High-level abstraction of APIs like BiDi or CDP.
 */
export interface IServer {
  setOnMessage: (handler: (messageObj: any) => Promise<void>) => void;
  sendMessage: (messageObj: any) => Promise<any>;
}

/**
 * Low-level abstraction of API like BiDi or CDP used by IServer implementation.
 */
export class ServerBinding {
  private _messageHandlerSetter: (
    messageHandler: (messageStr: string) => void
  ) => void;
  private _sendMessage: (messageStr: string) => void;

  constructor(
    sendMessage: (messageStr: string) => void,
    messageHandlerSetter: (messageHandler: (messageStr: string) => void) => void
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
