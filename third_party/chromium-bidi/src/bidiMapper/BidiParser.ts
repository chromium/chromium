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

import type {
  BrowsingContext,
  Cdp,
  Input,
  Network,
  Script,
  Session,
} from '../protocol/protocol.js';

export interface IBidiParser {
  // Browsing Context domain
  // keep-sorted start block=yes
  parseCaptureScreenshotParams(
    params: object
  ): BrowsingContext.CaptureScreenshotParameters;
  parseCloseParams(params: object): BrowsingContext.CloseParameters;
  parseCreateParams(params: object): BrowsingContext.CreateParameters;
  parseGetTreeParams(params: object): BrowsingContext.GetTreeParameters;
  parseHandleUserPromptParams(
    params: object
  ): BrowsingContext.HandleUserPromptParameters;
  parseNavigateParams(params: object): BrowsingContext.NavigateParameters;
  parsePrintParams(params: object): BrowsingContext.PrintParameters;
  parseReloadParams(params: object): BrowsingContext.ReloadParameters;
  parseSetViewportParams(params: object): BrowsingContext.SetViewportParameters;
  // keep-sorted end

  // CDP domain
  // keep-sorted start block=yes
  parseGetSessionParams(params: object): Cdp.GetSessionParams;
  parseSendCommandParams(params: object): Cdp.SendCommandParams;
  // keep-sorted end

  // Input domain
  // keep-sorted start block=yes
  parsePerformActionsParams(params: object): Input.PerformActionsParameters;
  parseReleaseActionsParams(params: object): Input.ReleaseActionsParameters;
  // keep-sorted end block=yes

  // Network domain
  // keep-sorted start block=yes
  parseAddInterceptParams(params: object): Network.AddInterceptParameters;
  parseContinueRequestParams(params: object): Network.ContinueRequestParameters;
  parseContinueResponseParams(
    params: object
  ): Network.ContinueResponseParameters;
  parseContinueWithAuthParams(
    params: object
  ): Network.ContinueWithAuthParameters;
  parseFailRequestParams(params: object): Network.FailRequestParameters;
  parseProvideResponseParams(params: object): Network.ProvideResponseParameters;
  parseRemoveInterceptParams(params: object): Network.RemoveInterceptParameters;
  // keep-sorted end

  // Script domain
  // keep-sorted start block=yes
  parseAddPreloadScriptParams(
    params: object
  ): Script.AddPreloadScriptParameters;
  parseCallFunctionParams(params: object): Script.CallFunctionParameters;
  parseDisownParams(params: object): Script.DisownParameters;
  parseEvaluateParams(params: object): Script.EvaluateParameters;
  parseGetRealmsParams(params: object): Script.GetRealmsParameters;
  parseRemovePreloadScriptParams(
    params: object
  ): Script.RemovePreloadScriptParameters;
  // keep-sorted end

  // Session domain
  // keep-sorted start block=yes
  parseSubscribeParams(params: object): Session.SubscriptionRequest;
  // keep-sorted end
}
