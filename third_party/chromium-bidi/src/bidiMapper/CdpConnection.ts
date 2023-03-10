/**
 * Copyright 2022 Google LLC.
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

/**
 * @fileoverview CDP interfaces and types that BiDi Mapper expects.
 */

import type {ProtocolMapping} from 'devtools-protocol/types/protocol-mapping.js';

import {EventEmitter} from '../utils/EventEmitter.js';

type CdpEvents = {
  [Property in keyof ProtocolMapping.Events]: ProtocolMapping.Events[Property][0];
};

export interface CdpConnection {
  browserClient(): CdpClient;
  getCdpClient(sessionId: string): CdpClient;
}

export interface CdpClient extends EventEmitter<CdpEvents> {
  sendCommand<CdpMethod extends keyof ProtocolMapping.Commands>(
    method: CdpMethod,
    ...params: ProtocolMapping.Commands[CdpMethod]['paramsType']
  ): Promise<ProtocolMapping.Commands[CdpMethod]['returnType']>;
}
