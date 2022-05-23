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

import { EventNames, Session as SessionType } from '../bidiProtocolTypes';
import { InvalidArgumentErrorResponse } from '../error';
import { BrowsingContextParser } from './browsingContextParser';
import { TypeHelper } from '../../../utils/typeHelper';

export namespace SessionParser {
  export namespace SubscribeCommand {
    export function parseParams(params: any): SessionType.SubscribeParameters {
      return {
        events: SessionParser.Events.parseList(params.events),
        // Don't add `contexts` if it's not defined.
        ...(params.contexts !== undefined && {
          contexts: BrowsingContextParser.BrowsingContext.parseOptionalList(
            params.contexts
          ),
        }),
      };
    }
  }

  export namespace Events {
    export function parseList(events: any): string[] {
      if (events === undefined)
        throw new InvalidArgumentErrorResponse('EventsList should be defined.');
      if (!Array.isArray(events))
        throw new InvalidArgumentErrorResponse(
          `EventsList should be an array. ${JSON.stringify(events)}.`
        );
      return events.map((e) => SessionParser.Events.parse(e));
    }

    export function parse(event: any): string {
      if (event === undefined)
        throw new InvalidArgumentErrorResponse(
          `Event should not be undefined.`
        );

      if (!TypeHelper.isString(event))
        throw new InvalidArgumentErrorResponse(
          `Event should be a string. ${JSON.stringify(event)}.`
        );

      if (!EventNames.has(event.toString()))
        throw new InvalidArgumentErrorResponse(
          `Unknown event. ${JSON.stringify(event)}.`
        );

      return event;
    }
  }
}
