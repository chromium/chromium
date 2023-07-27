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

import type * as WebDriverBidi from './webdriver-bidi.js';
import type * as Cdp from './cdp.js';

export type EventNames =
  // keep-sorted start
  | BrowsingContext.EventNames
  | Cdp.EventNames
  | Log.EventNames
  | Network.EventNames
  | Script.EventNames;
// keep-sorted end

export namespace Script {
  export enum EventNames {
    // keep-sorted start
    AllScriptEvent = 'script',
    MessageEvent = 'script.message',
    RealmCreated = 'script.realmCreated',
    RealmDestroyed = 'script.realmDestroyed',
    // keep-sorted end
  }
}

export namespace Log {
  export enum EventNames {
    AllLogEvent = 'log',
    LogEntryAddedEvent = 'log.entryAdded',
  }
}

export namespace BrowsingContext {
  export enum EventNames {
    // keep-sorted start
    AllBrowsingContextEvent = 'browsingContext',
    ContextCreatedEvent = 'browsingContext.contextCreated',
    ContextDestroyedEvent = 'browsingContext.contextDestroyed',
    DomContentLoadedEvent = 'browsingContext.domContentLoaded',
    FragmentNavigated = 'browsingContext.fragmentNavigated',
    LoadEvent = 'browsingContext.load',
    NavigationStarted = 'browsingContext.navigationStarted',
    UserPromptClosed = 'browsingContext.userPromptClosed',
    UserPromptOpened = 'browsingContext.userPromptOpened',
    // keep-sorted end
  }
}

export namespace Network {
  export enum EventNames {
    // keep-sorted start
    AllNetworkEvent = 'network',
    BeforeRequestSentEvent = 'network.beforeRequestSent',
    FetchErrorEvent = 'network.fetchError',
    ResponseCompletedEvent = 'network.responseCompleted',
    ResponseStartedEvent = 'network.responseStarted',
    // keep-sorted end
  }
}

export type Command = (WebDriverBidi.Command | Cdp.Command) & {
  channel?: WebDriverBidi.Script.Channel;
};

export type CommandResponse =
  | WebDriverBidi.CommandResponse
  | Cdp.CommandResponse;

export type Event = WebDriverBidi.Event | Cdp.Event;

export type ResultData = WebDriverBidi.ResultData | Cdp.ResultData;

export type Message = (
  | WebDriverBidi.Message
  | Cdp.Message
  | {launched: true}
) & {
  channel?: WebDriverBidi.Script.Channel;
};
