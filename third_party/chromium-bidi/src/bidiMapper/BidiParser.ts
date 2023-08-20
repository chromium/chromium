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
  parseActivateParams(params: unknown): BrowsingContext.ActivateParameters;
  parseCaptureScreenshotParams(
    params: unknown
  ): BrowsingContext.CaptureScreenshotParameters;
  parseCloseParams(params: unknown): BrowsingContext.CloseParameters;
  parseCreateParams(params: unknown): BrowsingContext.CreateParameters;
  parseGetTreeParams(params: unknown): BrowsingContext.GetTreeParameters;
  parseHandleUserPromptParams(
    params: unknown
  ): BrowsingContext.HandleUserPromptParameters;
  parseNavigateParams(params: unknown): BrowsingContext.NavigateParameters;
  parsePrintParams(params: unknown): BrowsingContext.PrintParameters;
  parseReloadParams(params: unknown): BrowsingContext.ReloadParameters;
  parseSetViewportParams(
    params: unknown
  ): BrowsingContext.SetViewportParameters;
  // keep-sorted end

  // CDP domain
  // keep-sorted start block=yes
  parseGetSessionParams(params: unknown): Cdp.GetSessionParameters;
  parseSendCommandParams(params: unknown): Cdp.SendCommandParameters;
  // keep-sorted end

  // Input domain
  // keep-sorted start block=yes
  parsePerformActionsParams(params: unknown): Input.PerformActionsParameters;
  parseReleaseActionsParams(params: unknown): Input.ReleaseActionsParameters;
  // keep-sorted end

  // Network domain
  // keep-sorted start block=yes
  parseAddInterceptParams(params: unknown): Network.AddInterceptParameters;
  parseContinueRequestParams(
    params: unknown
  ): Network.ContinueRequestParameters;
  parseContinueResponseParams(
    params: unknown
  ): Network.ContinueResponseParameters;
  parseContinueWithAuthParams(
    params: unknown
  ): Network.ContinueWithAuthParameters;
  parseFailRequestParams(params: unknown): Network.FailRequestParameters;
  parseProvideResponseParams(
    params: unknown
  ): Network.ProvideResponseParameters;
  parseRemoveInterceptParams(
    params: unknown
  ): Network.RemoveInterceptParameters;
  // keep-sorted end block=yes

  // Script domain
  // keep-sorted start block=yes
  parseAddPreloadScriptParams(
    params: unknown
  ): Script.AddPreloadScriptParameters;
  parseCallFunctionParams(params: unknown): Script.CallFunctionParameters;
  parseDisownParams(params: unknown): Script.DisownParameters;
  parseEvaluateParams(params: unknown): Script.EvaluateParameters;
  parseGetRealmsParams(params: unknown): Script.GetRealmsParameters;
  parseRemovePreloadScriptParams(
    params: unknown
  ): Script.RemovePreloadScriptParameters;
  // keep-sorted end

  // Session domain
  // keep-sorted start block=yes
  parseSubscribeParams(params: unknown): Session.SubscriptionRequest;
  // keep-sorted end
}
