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
import * as Parser from '../protocol-parser/protocol-parser.js';
import type {
  BrowsingContext,
  Cdp,
  Input,
  Network,
  Script,
  Session,
} from '../protocol/protocol';
import type {BidiParser} from '../bidiMapper/bidiMapper.js';

export class BidiParserImpl implements BidiParser {
  // Browsing Context domain
  parseCaptureScreenshotParams(
    params: object
  ): BrowsingContext.CaptureScreenshotParameters {
    return Parser.BrowsingContext.parseCaptureScreenshotParams(params);
  }
  parseCloseParams(params: object): BrowsingContext.CloseParameters {
    return Parser.BrowsingContext.parseCloseParams(params);
  }
  parseCreateParams(params: object): BrowsingContext.CreateParameters {
    return Parser.BrowsingContext.parseCreateParams(params);
  }
  parseGetTreeParams(params: object): BrowsingContext.GetTreeParameters {
    return Parser.BrowsingContext.parseGetTreeParams(params);
  }
  parseNavigateParams(params: object): BrowsingContext.NavigateParameters {
    return Parser.BrowsingContext.parseNavigateParams(params);
  }
  parsePrintParams(params: object): BrowsingContext.PrintParameters {
    return Parser.BrowsingContext.parsePrintParams(params);
  }
  parseReloadParams(params: object): BrowsingContext.ReloadParameters {
    return Parser.BrowsingContext.parseReloadParams(params);
  }
  parseSetViewportParams(
    params: object
  ): BrowsingContext.SetViewportParameters {
    return Parser.BrowsingContext.parseSetViewportParams(params);
  }

  // CDP domain
  parseGetSessionParams(params: object): Cdp.GetSessionParams {
    return Parser.Cdp.parseGetSessionParams(params);
  }
  parseSendCommandParams(params: object): Cdp.SendCommandParams {
    return Parser.Cdp.parseSendCommandParams(params);
  }

  // Input domain
  parsePerformActionsParams(params: object): Input.PerformActionsParameters {
    return Parser.Input.parsePerformActionsParams(params);
  }
  parseReleaseActionsParams(params: object): Input.ReleaseActionsParameters {
    return Parser.Input.parseReleaseActionsParams(params);
  }

  // Network domain
  parseAddInterceptParams(params: object): Network.AddInterceptParameters {
    return Parser.Network.parseAddInterceptParams(params);
  }
  parseContinueRequestParams(
    params: object
  ): Network.ContinueRequestParameters {
    return Parser.Network.parseContinueRequestParams(params);
  }
  parseContinueResponseParams(
    params: object
  ): Network.ContinueResponseParameters {
    return Parser.Network.parseContinueResponseParams(params);
  }
  parseContinueWithAuthParams(
    params: object
  ): Network.ContinueWithAuthParameters {
    return Parser.Network.parseContinueWithAuthParams(params);
  }
  parseFailRequestParams(params: object): Network.FailRequestParameters {
    return Parser.Network.parseFailRequestParams(params);
  }
  parseProvideResponseParams(
    params: object
  ): Network.ProvideResponseParameters {
    return Parser.Network.parseProvideResponseParams(params);
  }
  parseRemoveInterceptParams(
    params: object
  ): Network.RemoveInterceptParameters {
    return Parser.Network.parseRemoveInterceptParams(params);
  }

  // Script domain
  parseAddPreloadScriptParams(
    params: object
  ): Script.AddPreloadScriptParameters {
    return Parser.Script.parseAddPreloadScriptParams(params);
  }
  parseCallFunctionParams(params: object): Script.CallFunctionParameters {
    return Parser.Script.parseCallFunctionParams(params);
  }
  parseDisownParams(params: object): Script.DisownParameters {
    return Parser.Script.parseDisownParams(params);
  }
  parseEvaluateParams(params: object): Script.EvaluateParameters {
    return Parser.Script.parseEvaluateParams(params);
  }
  parseGetRealmsParams(params: object): Script.GetRealmsParameters {
    return Parser.Script.parseGetRealmsParams(params);
  }
  parseRemovePreloadScriptParams(
    params: object
  ): Script.RemovePreloadScriptParameters {
    return Parser.Script.parseRemovePreloadScriptParams(params);
  }

  // Session domain
  parseSubscribeParams(params: object): Session.SubscriptionRequest {
    return Parser.Session.parseSubscribeParams(params);
  }
}
