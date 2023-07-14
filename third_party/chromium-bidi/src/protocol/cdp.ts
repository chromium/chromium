/**
 * Copyright 2023 Google LLC.
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

import type {ProtocolMapping} from 'devtools-protocol/types/protocol-mapping.js';

import type {BrowsingContext, JsUint} from './webdriver-bidi.js';

export type Message = CommandResponse | Event;

export type Command = {
  id: JsUint;
} & CommandData;
export type CommandData = SendCommandCommand | GetSessionCommand;

export type CommandResponse = {
  type: 'success';
  id: JsUint;
  result: ResultData;
};
export type ResultData = SendCommandResult | GetSessionResult;

export type Event = {
  type: 'event';
} & EventData;
export type EventData = EventReceivedEvent;

export type SendCommandCommand = {
  method: 'cdp.sendCommand';
  params: SendCommandParameters;
};

export type SendCommandParameters<
  Command extends
    keyof ProtocolMapping.Commands = keyof ProtocolMapping.Commands,
> = {
  method: Command;
  params?: ProtocolMapping.Commands[Command]['paramsType'][0];
  session?: string;
};

export type SendCommandResult = {
  result: ProtocolMapping.Commands[keyof ProtocolMapping.Commands]['returnType'];
  session?: string;
};
export type GetSessionCommand = {
  method: 'cdp.getSession';
  params: GetSessionParameters;
};

export type GetSessionParameters = {
  context: BrowsingContext.BrowsingContext;
};

export type GetSessionResult = {
  session?: string;
};

export type EventReceivedEvent = {
  method: EventNames;
  params: EventParameters;
};

export type EventParameters<
  EventName extends keyof ProtocolMapping.Events = keyof ProtocolMapping.Events,
> = {
  event: EventName;
  params: ProtocolMapping.Events[EventName];
  session: string;
};

// XXX: Support `cdp` (all events)
export type EventNames = 'cdp' | `cdp.${keyof ProtocolMapping.Events}`;
