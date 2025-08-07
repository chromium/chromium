/**
 * Copyright 2025 Google LLC.
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

import type {Protocol} from 'devtools-protocol';

import type {
  BrowsingContext,
  Emulation,
  Session,
} from '../../../protocol/protocol.js';

/**
 * Represents a context configurations. It can be global, per User Context, or per
 * Browsing Context.
 */
export class ContextConfig {
  acceptInsecureCerts?: boolean;
  viewport?: BrowsingContext.Viewport | null;
  devicePixelRatio?: number | null;
  // Extra headers are kept in CDP format.
  extraHeaders?: Protocol.Network.Headers;
  geolocation?:
    | Emulation.GeolocationCoordinates
    | Emulation.GeolocationPositionError
    | null;
  locale?: string | null;
  prerenderingDisabled?: boolean;
  screenOrientation?: Emulation.ScreenOrientation | null;
  // Timezone is kept in CDP format with GMT prefix for offset values.
  timezone?: string | null;
  userPromptHandler?: Session.UserPromptHandler;
}
