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

import type {GoogCdp} from './generated/goog.js';

// The `goog:cdp` types are generated from `goog.cddl`. The types below refine
// them with the CDP commands and events from `devtools-protocol`.

/**
 * Narrows the generated `GoogCdp.SendCommandParameters`: `method` and `params`
 * to a known CDP command from `devtools-protocol`, for typed callers such as
 * Puppeteer. `session` is inherited.
 */
export interface SendCommandParameters<
  Command extends keyof ProtocolMapping.Commands =
    keyof ProtocolMapping.Commands,
>
  extends GoogCdp.SendCommandParameters {
  method: Command;
  params?: ProtocolMapping.Commands[Command]['paramsType'][0];
}

// Narrows `GoogCdp.EventParameters` for a specific CDP `EventName`; `session`
// is inherited.
interface EventParametersFor<EventName extends keyof ProtocolMapping.Events>
  extends GoogCdp.EventParameters {
  event: EventName;
  params: ProtocolMapping.Events[EventName][0];
}

// Narrows `GoogCdp.Event` for a specific CDP `EventName`, as CDDL cannot refer
// to `devtools-protocol`.
interface EventDataFor<EventName extends keyof ProtocolMapping.Events>
  extends GoogCdp.Event {
  method: `goog:cdp.${EventName}`;
  params: EventParametersFor<EventName>;
}

/**
 * Union of all `goog:cdp.<CDP event>` event payloads across
 * `ProtocolMapping.Events`, e.g. `goog:cdp.Page.loadEventFired`. Built by
 * `CdpTarget` and `CdpTargetManager` from the `CdpClient` `'*'` listener, and
 * wrapped into `ChromiumBidi.CdpEvent` (for `ChromiumBidi.Event`,
 * `ChromiumBidi.EventNames`, `ChromiumBidi.Message`, and Puppeteer); using
 * `GoogCdp.Event['method']` (`string`) directly would collapse
 * `ChromiumBidi.EventNames` to `string`.
 */
export type EventData = EventDataFor<keyof ProtocolMapping.Events>;
