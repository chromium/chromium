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

/**
 * @fileoverview Provides parsing and validator for WebDriver BiDi protocol.
 * Parser types should match the `../protocol` types.
 */
import {z, type ZodType} from 'zod';

import type * as Protocol from '../protocol/protocol.js';
import {InvalidArgumentException} from '../protocol/protocol.js';

import * as WebDriverBidiPermissions from './generated/webdriver-bidi-permissions.js';
import * as WebDriverBidi from './generated/webdriver-bidi.js';

export function parseObject<T extends ZodType>(
  obj: unknown,
  schema: T
): z.infer<T> {
  const parseResult = schema.safeParse(obj);
  if (parseResult.success) {
    return parseResult.data;
  }
  const errorMessage = parseResult.error.errors
    .map(
      (e) =>
        `${e.message} in ` +
        `${e.path.map((p: unknown) => JSON.stringify(p)).join('/')}.`
    )
    .join(' ');

  throw new InvalidArgumentException(errorMessage);
}

/** @see https://w3c.github.io/webdriver-bidi/#module-network */
export namespace Network {
  export function parseAddInterceptParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.AddInterceptParametersSchema
    );
  }

  export function parseContinueRequestParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.ContinueRequestParametersSchema
    );
  }

  export function parseContinueResponseParameters(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    // The generated schema `SameSiteSchema` in `src/protocol-parser/webdriver-bidi.ts` is
    // of type `"none" | "strict" | "lax"` which is not assignable to generated enum
    // `SameSite` in `src/protocol/webdriver-bidi.ts`.
    // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
    return parseObject(
      params,
      WebDriverBidi.Network.ContinueResponseParametersSchema
    ) as Protocol.Network.ContinueResponseParameters;
  }

  export function parseContinueWithAuthParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.ContinueWithAuthParametersSchema
    );
  }

  export function parseFailRequestParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.FailRequestParametersSchema
    );
  }

  export function parseProvideResponseParameters(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    // The generated schema `SameSiteSchema` in `src/protocol-parser/webdriver-bidi.ts` is
    // of type `"none" | "strict" | "lax"` which is not assignable to generated enum
    // `SameSite` in `src/protocol/webdriver-bidi.ts`.
    // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
    return parseObject(
      params,
      WebDriverBidi.Network.ProvideResponseParametersSchema
    ) as Protocol.Network.ProvideResponseParameters;
  }

  export function parseRemoveInterceptParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.RemoveInterceptParametersSchema
    );
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-script */
export namespace Script {
  export function parseGetRealmsParams(
    params: unknown
  ): Protocol.Script.GetRealmsParameters {
    return parseObject(params, WebDriverBidi.Script.GetRealmsParametersSchema);
  }

  export function parseEvaluateParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Script.EvaluateParametersSchema
    ) as Protocol.Script.EvaluateParameters;
  }

  export function parseDisownParams(
    params: unknown
  ): Protocol.Script.DisownParameters {
    return parseObject(params, WebDriverBidi.Script.DisownParametersSchema);
  }

  export function parseAddPreloadScriptParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Script.AddPreloadScriptParametersSchema
    ) as Protocol.Script.AddPreloadScriptParameters;
  }

  export function parseRemovePreloadScriptParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Script.RemovePreloadScriptParametersSchema
    );
  }

  export function parseCallFunctionParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Script.CallFunctionParametersSchema
    ) as Protocol.Script.CallFunctionParameters;
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-browsingContext */
export namespace BrowsingContext {
  export function parseActivateParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.ActivateParametersSchema
    );
  }

  export function parseGetTreeParams(
    params: unknown
  ): Protocol.BrowsingContext.GetTreeParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.GetTreeParametersSchema
    );
  }

  export function parseNavigateParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.NavigateParametersSchema
    ) as Protocol.BrowsingContext.NavigateParameters;
  }

  export function parseReloadParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.ReloadParametersSchema
    ) as Protocol.BrowsingContext.ReloadParameters;
  }

  export function parseCreateParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.CreateParametersSchema
    ) as Protocol.BrowsingContext.CreateParameters;
  }

  export function parseCloseParams(
    params: unknown
  ): Protocol.BrowsingContext.CloseParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.CloseParametersSchema
    );
  }

  export function parseCaptureScreenshotParams(
    params: unknown
  ): Protocol.BrowsingContext.CaptureScreenshotParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.CaptureScreenshotParametersSchema
    );
  }

  export function parsePrintParams(
    params: unknown
  ): Protocol.BrowsingContext.PrintParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.PrintParametersSchema
    );
  }

  export function parseSetViewportParams(
    params: unknown
  ): Protocol.BrowsingContext.SetViewportParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.SetViewportParametersSchema
    );
  }

  export function parseTraverseHistoryParams(
    params: unknown
  ): Protocol.BrowsingContext.TraverseHistoryParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.TraverseHistoryParametersSchema
    );
  }

  export function parseHandleUserPromptParameters(
    params: unknown
  ): Protocol.BrowsingContext.HandleUserPromptParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.HandleUserPromptParametersSchema
    );
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-session */
export namespace Session {
  export function parseSubscribeParams(
    params: unknown
  ): Protocol.Session.SubscriptionRequest {
    return parseObject(params, WebDriverBidi.Session.SubscriptionRequestSchema);
  }
}

export namespace Input {
  export function parsePerformActionsParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Input.PerformActionsParametersSchema
    ) as Protocol.Input.PerformActionsParameters;
  }

  export function parseReleaseActionsParams(
    params: unknown
  ): Protocol.Input.ReleaseActionsParameters {
    return parseObject(
      params,
      WebDriverBidi.Input.ReleaseActionsParametersSchema
    );
  }

  export function parseSetFilesParams(
    params: unknown
  ): Protocol.Input.SetFilesParameters {
    return parseObject(params, WebDriverBidi.Input.SetFilesParametersSchema);
  }
}

export namespace Storage {
  export function parseGetCookiesParams(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    // The generated schema `SameSiteSchema` in `src/protocol-parser/webdriver-bidi.ts` is
    // of type `"none" | "strict" | "lax"` which is not assignable to generated enum
    // `SameSite` in `src/protocol/webdriver-bidi.ts`.
    // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
    return parseObject(
      params,
      WebDriverBidi.Storage.GetCookiesParametersSchema
    ) as Protocol.Storage.GetCookiesParameters;
  }

  export function parseSetCookieParams(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    // The generated schema `SameSiteSchema` in `src/protocol-parser/webdriver-bidi.ts` is
    // of type `"none" | "strict" | "lax"` which is not assignable to generated enum
    // `SameSite` in `src/protocol/webdriver-bidi.ts`.
    // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
    return parseObject(
      params,
      WebDriverBidi.Storage.SetCookieParametersSchema
    ) as Protocol.Storage.SetCookieParameters;
  }

  export function parseDeleteCookiesParams(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    // The generated schema `SameSiteSchema` in `src/protocol-parser/webdriver-bidi.ts` is
    // of type `"none" | "strict" | "lax"` which is not assignable to generated enum
    // `SameSite` in `src/protocol/webdriver-bidi.ts`.
    // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
    return parseObject(
      params,
      WebDriverBidi.Storage.DeleteCookiesParametersSchema
    ) as Protocol.Storage.DeleteCookiesParameters;
  }
}

export namespace Cdp {
  const SendCommandRequestSchema = z.object({
    // Allowing any cdpMethod, and casting to proper type later on.
    method: z.string(),
    // `passthrough` allows object to have any fields.
    // https://github.com/colinhacks/zod#passthrough
    params: z.object({}).passthrough().optional(),
    session: z.string().optional(),
  });

  const GetSessionRequestSchema = z.object({
    context: WebDriverBidi.BrowsingContext.BrowsingContextSchema,
  });

  export function parseSendCommandRequest(
    params: unknown
  ): Protocol.Cdp.SendCommandParameters {
    return parseObject(
      params,
      SendCommandRequestSchema
    ) as Protocol.Cdp.SendCommandParameters;
  }

  export function parseGetSessionRequest(
    params: unknown
  ): Protocol.Cdp.GetSessionParameters {
    return parseObject(params, GetSessionRequestSchema);
  }
}

export namespace Permissions {
  export function parseSetPermissionsParams(
    params: unknown
  ): Protocol.Permissions.SetPermissionParameters {
    return {
      // TODO: remove once "goog:" attributes are not needed.
      ...(params as object),
      ...(parseObject(
        params,
        WebDriverBidiPermissions.Permissions.SetPermissionParametersSchema
      ) as Protocol.Permissions.SetPermissionParameters),
    };
  }
}
