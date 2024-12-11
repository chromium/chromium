/*
 *  Copyright 2024 Google LLC.
 *  Copyright (c) Microsoft Corporation.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

/**
 * `FrameStartedNavigating` event addressing lack of such an event in CDP. It is emitted
 * on CdpTarget before each `Network.requestWillBeSent` event. Note that there can be
 * several `Network.requestWillBeSent` events for a single navigation e.g. on redirection,
 * so the `FrameStartedNavigating` can be duplicated as well.
 * http://go/webdriver:detect-navigation-started#bookmark=id.64balpqrmadv
 */
export const enum TargetEvents {
  FrameStartedNavigating = 'frameStartedNavigating',
}

export type TargetEventMap = {
  [TargetEvents.FrameStartedNavigating]: {
    loaderId: string;
    url: string;
    frameId: string | undefined;
  };
};
