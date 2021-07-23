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
import { log } from '../utils/log';
const logCdp = log('cdp');

import { EventEmitter } from 'events';
import { IServer } from '../utils/iServer';

import * as browserProtocol from 'devtools-protocol/json/browser_protocol.json';
import * as jsProtocol from 'devtools-protocol/json/js_protocol.json';
import ProtocolProxyApi from 'devtools-protocol/types/protocol-proxy-api';

// Publicly visible type. Has all of the methods of CdpClientImpl, and a property
// getter for each CDP Domain (provided by ProtocolApiExt).
export type CdpClient = CdpClientImpl & ProtocolApiExt;

// A type with the same set of properties as ProtocolProxyApi, but each domain
// property also extends DomainImpl.
type ProtocolApiExt = {
  [Domain in keyof ProtocolProxyApi.ProtocolApi]: DomainImpl &
    ProtocolProxyApi.ProtocolApi[Domain];
};

export interface CdpError {
  code: number;
  message: string;
}

interface CdpMessage {
  id?: number;
  result?: {};
  error?: {};
  method?: string;
  params?: {};
}

interface CdpCallbacks {
  resolve: (messageObj: {}) => void;
  reject: (errorObj: {}) => void;
}

const mergedProtocol = [...browserProtocol.domains, ...jsProtocol.domains];

// Generate classes for each Domain and store constructors here.
const domainConstructorMap = new Map<
  string,
  { new (client: CdpClientImpl): DomainImpl }
>();

// Base class for all domains.
class DomainImpl extends EventEmitter {
  constructor(private _client: CdpClientImpl) {
    super();
  }
}

for (let domainInfo of mergedProtocol) {
  // Dynamically create a subclass for this domain. Note: This class definition is scoped
  // to this for-loop, so there will be a unique ThisDomain definition for each domain.
  class ThisDomain extends DomainImpl {
    constructor(_client: CdpClientImpl) {
      super(_client);
    }
  }

  // Add methods to our Domain for each available command.
  for (let command of domainInfo.commands) {
    Object.defineProperty(ThisDomain.prototype, command.name, {
      value: async function (params: {}) {
        return await this._client.sendCommand(
          `${domainInfo.domain}.${command.name}`,
          params
        );
      },
    });
  }

  domainConstructorMap.set(domainInfo.domain, ThisDomain);
}

class CdpClientImpl extends EventEmitter {
  private _commandCallbacks: Map<number, CdpCallbacks>;
  private _domains: Map<string, DomainImpl>;
  private _nextId: number;

  constructor(private _transport: IServer) {
    super();

    this._commandCallbacks = new Map();
    this._nextId = 0;
    this._transport.setOnMessage(this._onCdpMessage.bind(this));

    this._domains = new Map();
    for (const [domainName, ctor] of domainConstructorMap.entries()) {
      this._domains.set(domainName, new ctor(this));
      Object.defineProperty(this, domainName, {
        get(this: CdpClientImpl) {
          return this._domains.get(domainName);
        },
      });
    }
  }

  /**
   * Returns command promise, which will be resolved wth the command result after receiving CDP result.
   * @param method Name of the CDP command to call.
   * @param params Parameters to pass to the CDP command.
   */
  sendCommand(method: string, params: {}): Promise<{}> {
    return new Promise((resolve, reject) => {
      const id = this._nextId++;
      this._commandCallbacks.set(id, { resolve, reject });
      const messageObj = { id, method, params };
      const messageStr = JSON.stringify(messageObj);

      logCdp('sent > ' + messageStr);
      this._transport.sendMessage(messageStr);
    });
  }

  private _onCdpMessage(messageStr: string): void {
    logCdp('received < ' + messageStr);

    const messageObj: CdpMessage = JSON.parse(messageStr);
    if (messageObj.id !== undefined) {
      // Handle command response.
      const callbacks = this._commandCallbacks.get(messageObj.id);
      if (callbacks) {
        if (messageObj.result) {
          callbacks.resolve(messageObj.result);
        } else if (messageObj.error) {
          callbacks.reject(messageObj.error);
        }
      }
    } else if (messageObj.method) {
      // Emit a generic "event" event from here that includes the method name. Useful as a catch-all.
      this.emit('event', messageObj.method, messageObj.params);

      // Next, get the correct domain instance and tell it to emit the strongly typed event.
      const [domainName, eventName] = messageObj.method.split('.');
      this._domains.get(domainName).emit(eventName, messageObj.params);
    }
  }
}

/**
 * Creates a new CDP client object that communicates with the browser using a given
 * transport mechanism.
 * @param transport A transport object that will be used to send and receive raw CDP messages.
 * @returns A connected CDP client object.
 */
export function connectCdp(transport: IServer) {
  return new CdpClientImpl(transport) as unknown as CdpClient;
}
