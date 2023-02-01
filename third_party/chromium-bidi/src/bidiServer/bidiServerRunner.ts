/**
 * Copyright 2021 Google LLC.
 * Copyright (c) Microsoft Corporation.
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
import {ITransport} from '../utils/transport.js';
import debug from 'debug';
import http from 'http';
import websocket from 'websocket';
const debugInternal = debug('bidiServer:internal');
const debugSend = debug('bidiServer:SEND ►');
const debugRecv = debug('bidiServer:RECV ◀');

async function getHttpRequestPayload(
  request: http.IncomingMessage
): Promise<string> {
  return new Promise((resolve, reject) => {
    let data = '';
    request.on('data', (chunk) => {
      data += chunk;
    });
    request.on('end', () => {
      resolve(data);
    });
    request.on('error', (error) => {
      reject(error);
    });
  });
}

export class BidiServerRunner {
  /**
   *
   * @param bidiPort port to start ws server on
   * @param onNewBidiConnectionOpen delegate to be called for each new
   * connection. `onNewBidiConnectionOpen` delegate should return another
   * `onConnectionClose` delegate, which will be called after the connection is
   * closed.
   */
  run(
    bidiPort: number,
    onNewBidiConnectionOpen: (bidiServer: ITransport) => Promise<() => void>
  ) {
    const server = http.createServer(
      async (request: http.IncomingMessage, response: http.ServerResponse) => {
        debugInternal(
          `${new Date()} Received ${request.method} request for ${request.url}`
        );
        if (!request.url) return response.end(404);

        // https://w3c.github.io/webdriver-bidi/#transport, step 2.
        if (request.url === '/session') {
          response.writeHead(200, {
            'Content-Type': 'application/json;charset=utf-8',
            'Cache-Control': 'no-cache',
          });
          response.write(
            JSON.stringify({
              value: {
                sessionId: '1',
                capabilities: {
                  webSocketUrl: `ws://localhost:${bidiPort}`,
                },
              },
            })
          );
        } else if (request.url.startsWith('/session')) {
          debugInternal(
            `Unknown session command ${JSON.stringify(
              request.method
            )} request for ${JSON.stringify(
              request.url
            )} with payload ${JSON.stringify(
              await getHttpRequestPayload(request)
            )}. 200 returned.`
          );

          response.writeHead(200, {
            'Content-Type': 'application/json;charset=utf-8',
            'Cache-Control': 'no-cache',
          });
          response.write(
            JSON.stringify({
              value: {},
            })
          );
        } else {
          debugInternal(
            `Unknown ${JSON.stringify(
              request.method
            )} request for ${JSON.stringify(
              request.url
            )} with payload ${JSON.stringify(
              await getHttpRequestPayload(request)
            )}. 404 returned.`
          );
          response.writeHead(404);
        }
        return response.end();
      }
    );
    server.listen(bidiPort, () => {
      console.log(`Server is listening on port ${bidiPort}`);
    });

    const wsServer: websocket.server = new websocket.server({
      httpServer: server,
      autoAcceptConnections: false,
    });

    wsServer.on('request', async (request: websocket.request) => {
      debugInternal(
        `new WS request received: ${JSON.stringify(request.resourceURL.path)}`
      );

      const bidiServer = new BidiServer();

      const onBidiConnectionClosed = await onNewBidiConnectionOpen(bidiServer);

      const connection = request.accept();

      connection.on('message', (message) => {
        // 1. If |type| is not text, return.
        if (message.type !== 'utf8') {
          this.#respondWithError(
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

      connection.on('close', () => {
        debugInternal(
          `${new Date()} Peer ${connection.remoteAddress} disconnected.`
        );

        onBidiConnectionClosed();
      });

      bidiServer.initialise((messageStr) => {
        return this.#sendClientMessageStr(messageStr, connection);
      });
    });
  }

  #sendClientMessageStr(
    messageStr: string,
    connection: websocket.connection
  ): Promise<void> {
    debugSend(messageStr);
    connection.sendUTF(messageStr);
    return Promise.resolve();
  }

  #sendClientMessage(
    messageObj: unknown,
    connection: websocket.connection
  ): Promise<void> {
    const messageStr = JSON.stringify(messageObj);
    return this.#sendClientMessageStr(messageStr, connection);
  }

  #respondWithError(
    connection: websocket.connection,
    plainCommandData: unknown,
    errorCode: string,
    errorMessage: string
  ) {
    const errorResponse = this.#getErrorResponse(
      plainCommandData,
      errorCode,
      errorMessage
    );
    this.#sendClientMessage(errorResponse, connection);
  }

  #getErrorResponse(
    plainCommandData: any,
    errorCode: string,
    errorMessage: string
  ) {
    // TODO: this is bizarre per spec. We reparse the payload and
    // extract the ID, regardless of what kind of value it was.
    let commandId;
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

class BidiServer implements ITransport {
  #handlers: ((message: string) => void)[] = [];
  #sendBidiMessage: ((message: string) => Promise<void>) | null = null;

  setOnMessage(handler: (messageStr: string) => Promise<void>): void {
    this.#handlers.push(handler);
  }

  sendMessage(message: string): Promise<void> {
    if (!this.#sendBidiMessage)
      throw new Error('Bidi connection is not initialised yet');

    return this.#sendBidiMessage(message);
  }

  close() {}

  initialise(sendBidiMessage: (messageStr: string) => Promise<void>): void {
    this.#sendBidiMessage = sendBidiMessage;
  }

  onMessage(messageStr: string): void {
    for (const handler of this.#handlers) handler(messageStr);
  }
}
