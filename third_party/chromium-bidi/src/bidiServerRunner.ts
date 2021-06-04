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
import websocket from 'websocket';
import http from 'http';
import { IServer } from './iServer';

import debug from 'debug';
const debugInternal = debug('bidiServer:internal');
const debugSend = debug('bidiServer:SEND ►');
const debugRecv = debug('bidiServer:RECV ◀');

export class BidiServerRunner {
  // Once run, waits for connection. For each connection, calls `onOpen` and stores the returned `stateObject`.
  // `stateObject` passed to `onClose` afterwards.
  static run(
    onOpen: (bidiServer: IServer) => Promise<any>,
    onClose: (stateObject: any) => void
  ) {
    const self = this;
    const bidiPort = parseInt(process.env.PORT) || 8080;

    const server = http.createServer(function (request, response) {
      debugInternal(new Date() + ' Received request for ' + request.url);
      response.writeHead(404);
      response.end();
    });
    server.listen(bidiPort, function () {
      debugInternal(`${new Date()} Server is listening on port ${bidiPort}`);
    });

    const wsServer = new websocket.server({
      httpServer: server,
      autoAcceptConnections: false,
    });

    wsServer.on('request', async function (request) {
      debugInternal('request received');

      const bidiServer = new BidiServer();

      // `stateObj` is used to store browser reference. Returned by `onOpen`, and passed to `onClose`.
      const stateObj = await onOpen(bidiServer);

      const connection = request.accept();

      connection.on('message', function (message) {
        // 1. If |type| is not text, return.
        if (message.type !== 'utf8') {
          self._respondWithError(
            connection,
            {},
            'invalid argument',
            `not supported type (${message.type})`
          );
          return;
        }

        const plainCommandData = message.utf8Data;

        debugRecv(plainCommandData);
        bidiServer.onMessage(plainCommandData);
      });

      connection.on('close', function () {
        debugInternal(
          new Date() + ' Peer ' + connection.remoteAddress + ' disconnected.'
        );

        onClose(stateObj);
      });

      bidiServer.initialise((messageStr) => {
        return self._sendClientMessageStr(messageStr, connection);
      });
    });
  }

  private static _sendClientMessageStr(
    messageStr: string,
    connection: websocket.connection
  ): Promise<void> {
    debugSend(messageStr);
    connection.sendUTF(messageStr);
    return Promise.resolve();
  }
  private static _sendClientMessage(
    messageObj: any,
    connection: websocket.connection
  ): Promise<void> {
    const messageStr = JSON.stringify(messageObj);
    return this._sendClientMessageStr(messageStr, connection);
  }

  private static _respondWithError(
    connection: websocket.connection,
    plainCommandData: any,
    errorCode: string,
    errorMessage: string
  ) {
    const errorResponse = this._getErrorResponse(
      plainCommandData,
      errorCode,
      errorMessage
    );
    this._sendClientMessage(errorResponse, connection);
  }

  private static _getErrorResponse(
    plainCommandData: any,
    errorCode: string,
    errorMessage: string
  ): any {
    // TODO: this is bizarre per spec. We reparse the payload and
    // extract the ID, regardless of what kind of value it was.
    let commandId = undefined;
    try {
      const commandData = JSON.parse(plainCommandData);
      if ('id' in commandData) {
        commandId = commandData.id;
      }
    } catch {}

    return {
      id: commandId,
      error: errorCode,
      message: errorMessage,
      // TODO: optional stacktrace field.
    };
  }
}

class BidiServer implements IServer {
  private _handlers: ((messageStr: string) => void)[] = new Array();
  private _initialised: boolean = false;
  private _sendBidiMessage: (messageStr: string) => Promise<void>;

  setOnMessage(handler: (messageStr: string) => Promise<void>): void {
    this._handlers.push(handler);
  }
  sendMessage(messageStr: any): Promise<void> {
    if (!this._initialised)
      throw new Error('Bidi connection is not initialised yet');

    return this._sendBidiMessage(messageStr);
  }

  initialise(sendBidiMessage: (messageStr: string) => Promise<void>): void {
    this._initialised = true;
    this._sendBidiMessage = sendBidiMessage;
  }

  onMessage(messageStr: string): void {
    for (let handler of this._handlers) handler(messageStr);
  }
}
