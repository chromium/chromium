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

import {EventEmitter} from 'events';
import {CdpConnection} from './cdpConnection';

import {domains as browserProtocolDomains} from 'devtools-protocol/json/browser_protocol_commands_only.json';
import {domains as jsProtocolDomains} from 'devtools-protocol/json/js_protocol_commands_only.json';
import ProtocolProxyApi from 'devtools-protocol/types/protocol-proxy-api';
import ProtocolMapping from 'devtools-protocol/types/protocol-mapping';

// Publicly visible type. Has all of the methods of CdpClientImpl, and a property
// getter for each CDP Domain (provided by ProtocolApiExt).
export type CdpClient = CdpClientImpl & ProtocolApiExt;

// A type with the same set of properties as ProtocolProxyApi, but each domain
// property also extends DomainImpl.
type ProtocolApiExt = {
  [Domain in keyof ProtocolProxyApi.ProtocolApi]: DomainImpl &
    ProtocolProxyApi.ProtocolApi[Domain];
};

const browserAndJsProtocolDomains = [
  ...browserProtocolDomains,
  ...jsProtocolDomains,
];

// Generate classes for each Domain and store constructors here.
const domainConstructorMap = new Map<
  string,
  {new (client: CdpClientImpl): DomainImpl}
>();

// Base class for all domains.
class DomainImpl extends EventEmitter {
  // @ts-ignore
  constructor(private _client: CdpClientImpl) {
    super();
  }
}

for (let domains of browserAndJsProtocolDomains) {
  // Dynamically create a subclass for this domain. Note: This class definition is scoped
  // to this for-loop, so there will be a unique ThisDomain definition for each domain.
  class ThisDomain extends DomainImpl {
    constructor(_client: CdpClientImpl) {
      super(_client);
    }
  }

  // Add methods to our Domain for each available command.
  for (let command of domains.commands) {
    Object.defineProperty(ThisDomain.prototype, command, {
      value: async function (params: object) {
        return await this._client.sendCommand(
          `${domains.domain}.${command}`,
          params
        );
      },
    });
  }

  domainConstructorMap.set(domains.domain, ThisDomain);
}

interface CdpClientImpl {
  on<K extends keyof ProtocolMapping.Events>(
    event: 'event',
    listener: (method: K, params: object) => void
  ): this;
}

class CdpClientImpl extends EventEmitter {
  private _domains: Map<string, DomainImpl>;

  constructor(
    private _cdpConnection: CdpConnection,
    private _sessionId: string | null
  ) {
    super();

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
  sendCommand(method: string, params: object): Promise<object> {
    return this._cdpConnection.sendCommand(method, params, this._sessionId);
  }

  _onCdpEvent(method: string, params: object) {
    // Emit a generic "event" event from here that includes the method name. Useful as a catch-all.
    this.emit('event', method, params);

    // Next, get the correct domain instance and tell it to emit the strongly typed event.
    const [domainName, eventName] = method.split('.');
    if (!domainName || !eventName) {
      throw new Error('Malformed message');
    } 
    const domain = this._domains.get(domainName);
    if (domain) {
      domain.emit(eventName, params);
    }
  }
}

/**
 * Creates a new CDP client object that communicates with the browser using a given
 * transport mechanism.
 * @param transport A transport object that will be used to send and receive raw CDP messages.
 * @returns A connected CDP client object.
 */
export function createClient(
  cdpConnection: CdpConnection,
  sessionId: string | null
) {
  return new CdpClientImpl(cdpConnection, sessionId) as unknown as CdpClient;
}
