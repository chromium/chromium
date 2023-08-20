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
import type {BidiParser} from '../bidiMapper/bidiMapper.js';
import * as Parser from '../protocol-parser/protocol-parser.js';
import type {
  BrowsingContext,
  Cdp,
  Input,
  Network,
  Script,
  Session,
} from '../protocol/protocol';

export class BidiParserImpl implements BidiParser {
  // Browsing Context domain
  // keep-sorted start block=yes
  parseActivateParams(params: unknown): BrowsingContext.ActivateParameters {
    return Parser.BrowsingContext.parseActivateParams(params);
  }
  parseCaptureScreenshotParams(
    params: unknown
  ): BrowsingContext.CaptureScreenshotParameters {
    return Parser.BrowsingContext.parseCaptureScreenshotParams(params);
  }
  parseCloseParams(params: unknown): BrowsingContext.CloseParameters {
    return Parser.BrowsingContext.parseCloseParams(params);
  }
  parseCreateParams(params: unknown): BrowsingContext.CreateParameters {
    return Parser.BrowsingContext.parseCreateParams(params);
  }
  parseGetTreeParams(params: unknown): BrowsingContext.GetTreeParameters {
    return Parser.BrowsingContext.parseGetTreeParams(params);
  }
  parseHandleUserPromptParams(
    params: unknown
  ): BrowsingContext.HandleUserPromptParameters {
    return Parser.BrowsingContext.parseHandleUserPromptParameters(params);
  }
  parseNavigateParams(params: unknown): BrowsingContext.NavigateParameters {
    return Parser.BrowsingContext.parseNavigateParams(params);
  }
  parsePrintParams(params: unknown): BrowsingContext.PrintParameters {
    return Parser.BrowsingContext.parsePrintParams(params);
  }
  parseReloadParams(params: unknown): BrowsingContext.ReloadParameters {
    return Parser.BrowsingContext.parseReloadParams(params);
  }
  parseSetViewportParams(
    params: unknown
  ): BrowsingContext.SetViewportParameters {
    return Parser.BrowsingContext.parseSetViewportParams(params);
  }
  // keep-sorted end

  // CDP domain
  // keep-sorted start block=yes
  parseGetSessionParams(params: unknown): Cdp.GetSessionParameters {
    return Parser.Cdp.parseGetSessionRequest(params);
  }
  parseSendCommandParams(params: unknown): Cdp.SendCommandParameters {
    return Parser.Cdp.parseSendCommandRequest(params);
  }
  // keep-sorted end

  // Input domain
  // keep-sorted start block=yes
  parsePerformActionsParams(params: unknown): Input.PerformActionsParameters {
    return Parser.Input.parsePerformActionsParams(params);
  }
  parseReleaseActionsParams(params: unknown): Input.ReleaseActionsParameters {
    return Parser.Input.parseReleaseActionsParams(params);
  }
  // keep-sorted end

  // Network domain
  // keep-sorted start block=yes
  parseAddInterceptParams(params: unknown): Network.AddInterceptParameters {
    return Parser.Network.parseAddInterceptParameters(
      params
    ) as Network.AddInterceptParameters;
  }
  parseContinueRequestParams(
    params: unknown
  ): Network.ContinueRequestParameters {
    return Parser.Network.parseContinueRequestParameters(params);
  }
  parseContinueResponseParams(
    params: unknown
  ): Network.ContinueResponseParameters {
    return Parser.Network.parseContinueResponseParameters(params);
  }
  parseContinueWithAuthParams(
    params: unknown
  ): Network.ContinueWithAuthParameters {
    return Parser.Network.parseContinueWithAuthParameters(params);
  }
  parseFailRequestParams(params: unknown): Network.FailRequestParameters {
    return Parser.Network.parseFailRequestParameters(params);
  }
  parseProvideResponseParams(
    params: unknown
  ): Network.ProvideResponseParameters {
    return Parser.Network.parseProvideResponseParameters(params);
  }
  parseRemoveInterceptParams(
    params: unknown
  ): Network.RemoveInterceptParameters {
    return Parser.Network.parseRemoveInterceptParameters(params);
  }
  // keep-sorted end

  // Script domain
  // keep-sorted start block=yes
  parseAddPreloadScriptParams(
    params: unknown
  ): Script.AddPreloadScriptParameters {
    return Parser.Script.parseAddPreloadScriptParams(params);
  }
  parseCallFunctionParams(params: unknown): Script.CallFunctionParameters {
    return Parser.Script.parseCallFunctionParams(params);
  }
  parseDisownParams(params: unknown): Script.DisownParameters {
    return Parser.Script.parseDisownParams(params);
  }
  parseEvaluateParams(params: unknown): Script.EvaluateParameters {
    return Parser.Script.parseEvaluateParams(params);
  }
  parseGetRealmsParams(params: unknown): Script.GetRealmsParameters {
    return Parser.Script.parseGetRealmsParams(params);
  }
  parseRemovePreloadScriptParams(
    params: unknown
  ): Script.RemovePreloadScriptParameters {
    return Parser.Script.parseRemovePreloadScriptParams(params);
  }
  // keep-sorted end

  // Session domain
  // keep-sorted start block=yes
  parseSubscribeParams(params: unknown): Session.SubscriptionRequest {
    return Parser.Session.parseSubscribeParams(params);
  }
  // keep-sorted end
}
