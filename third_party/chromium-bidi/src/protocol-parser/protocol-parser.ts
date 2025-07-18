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

import * as WebDriverBidiBluetooth from './generated/webdriver-bidi-bluetooth.js';
import * as WebDriverBidiPermissions from './generated/webdriver-bidi-permissions.js';
import * as WebDriverBidi from './generated/webdriver-bidi.js';

export function parseObject<T extends ZodType>(
  obj: unknown,
  schema: T,
): z.infer<T> {
  const parseResult = schema.safeParse(obj);
  if (parseResult.success) {
    return parseResult.data;
  }
  const errorMessage = parseResult.error.errors
    .map(
      (e) =>
        `${e.message} in ` +
        `${e.path.map((p: unknown) => JSON.stringify(p)).join('/')}.`,
    )
    .join(' ');

  throw new InvalidArgumentException(errorMessage);
}

/** @see https://w3c.github.io/webdriver-bidi/#module-browser */
export namespace Browser {
  export function parseCreateUserContextParameters(
    params: unknown,
  ): Protocol.Browser.CreateUserContextParameters {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    return parseObject(
      params,
      WebDriverBidi.Browser.CreateUserContextParametersSchema,
    ) as Protocol.Browser.CreateUserContextParameters;
  }
  export function parseRemoveUserContextParameters(
    params: unknown,
  ): Protocol.Browser.RemoveUserContextParameters {
    return parseObject(
      params,
      WebDriverBidi.Browser.RemoveUserContextParametersSchema,
    );
  }
  export function parseSetClientWindowStateParameters(
    params: unknown,
  ): Protocol.Browser.SetClientWindowStateParameters {
    return parseObject(
      params,
      WebDriverBidi.Browser.SetClientWindowStateParametersSchema,
    );
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-network */
export namespace Network {
  export function parseAddDataCollectorParameters(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    return parseObject(
      params,
      WebDriverBidi.Network.AddDataCollectorParametersSchema,
    ) as Protocol.Network.AddDataCollectorParameters;
  }

  export function parseAddInterceptParameters(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    return parseObject(
      params,
      WebDriverBidi.Network.AddInterceptParametersSchema,
    ) as Protocol.Network.AddInterceptParameters;
  }

  export function parseContinueRequestParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.ContinueRequestParametersSchema,
    );
  }

  export function parseContinueResponseParameters(params: unknown) {
    // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
    return parseObject(
      params,
      WebDriverBidi.Network.ContinueResponseParametersSchema,
    ) as Protocol.Network.ContinueResponseParameters;
  }

  export function parseContinueWithAuthParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.ContinueWithAuthParametersSchema,
    );
  }

  export function parseFailRequestParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.FailRequestParametersSchema,
    );
  }

  export function parseGetDataParameters(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    return parseObject(
      params,
      WebDriverBidi.Network.GetDataParametersSchema,
    ) as Protocol.Network.GetDataParameters;
  }
  export function parseDisownDataParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.DisownDataParametersSchema,
    ) as Protocol.Network.DisownDataParameters;
  }

  export function parseProvideResponseParameters(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    return parseObject(
      params,
      WebDriverBidi.Network.ProvideResponseParametersSchema,
    ) as Protocol.Network.ProvideResponseParameters;
  }

  export function parseRemoveDataCollectorParameters(params: unknown) {
    // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
    return parseObject(
      params,
      WebDriverBidi.Network.RemoveDataCollectorParametersSchema,
    ) as Protocol.Network.RemoveDataCollectorParameters;
  }

  export function parseRemoveInterceptParameters(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.RemoveInterceptParametersSchema,
    );
  }

  export function parseSetCacheBehavior(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Network.SetCacheBehaviorParametersSchema,
    ) as Protocol.Network.SetCacheBehaviorParameters;
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-script */
export namespace Script {
  export function parseGetRealmsParams(
    params: unknown,
  ): Protocol.Script.GetRealmsParameters {
    return parseObject(params, WebDriverBidi.Script.GetRealmsParametersSchema);
  }

  export function parseEvaluateParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Script.EvaluateParametersSchema,
    ) as Protocol.Script.EvaluateParameters;
  }

  export function parseDisownParams(
    params: unknown,
  ): Protocol.Script.DisownParameters {
    return parseObject(params, WebDriverBidi.Script.DisownParametersSchema);
  }

  export function parseAddPreloadScriptParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Script.AddPreloadScriptParametersSchema,
    ) as Protocol.Script.AddPreloadScriptParameters;
  }

  export function parseRemovePreloadScriptParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Script.RemovePreloadScriptParametersSchema,
    );
  }

  export function parseCallFunctionParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Script.CallFunctionParametersSchema,
    ) as Protocol.Script.CallFunctionParameters;
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-browsingContext */
export namespace BrowsingContext {
  export function parseActivateParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.ActivateParametersSchema,
    );
  }

  export function parseGetTreeParams(
    params: unknown,
  ): Protocol.BrowsingContext.GetTreeParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.GetTreeParametersSchema,
    );
  }

  export function parseNavigateParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.NavigateParametersSchema,
    ) as Protocol.BrowsingContext.NavigateParameters;
  }

  export function parseReloadParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.ReloadParametersSchema,
    ) as Protocol.BrowsingContext.ReloadParameters;
  }

  export function parseCreateParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.CreateParametersSchema,
    ) as Protocol.BrowsingContext.CreateParameters;
  }

  export function parseCloseParams(
    params: unknown,
  ): Protocol.BrowsingContext.CloseParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.CloseParametersSchema,
    );
  }

  export function parseCaptureScreenshotParams(
    params: unknown,
  ): Protocol.BrowsingContext.CaptureScreenshotParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.CaptureScreenshotParametersSchema,
    );
  }

  export function parsePrintParams(
    params: unknown,
  ): Protocol.BrowsingContext.PrintParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.PrintParametersSchema,
    );
  }

  export function parseSetViewportParams(
    params: unknown,
  ): Protocol.BrowsingContext.SetViewportParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.SetViewportParametersSchema,
    ) as Protocol.BrowsingContext.SetViewportParameters;
  }

  export function parseTraverseHistoryParams(
    params: unknown,
  ): Protocol.BrowsingContext.TraverseHistoryParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.TraverseHistoryParametersSchema,
    );
  }

  export function parseHandleUserPromptParameters(
    params: unknown,
  ): Protocol.BrowsingContext.HandleUserPromptParameters {
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.HandleUserPromptParametersSchema,
    );
  }

  export function parseLocateNodesParams(
    params: unknown,
  ): Protocol.BrowsingContext.LocateNodesParameters {
    // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
    return parseObject(
      params,
      WebDriverBidi.BrowsingContext.LocateNodesParametersSchema,
    ) as Protocol.BrowsingContext.LocateNodesParameters;
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-session */
export namespace Session {
  export function parseSubscribeParams(
    params: unknown,
  ): Protocol.Session.SubscriptionRequest {
    return parseObject(
      params,
      WebDriverBidi.Session.SubscriptionRequestSchema,
    ) as Protocol.Session.SubscriptionRequest;
  }

  export function parseUnsubscribeParams(
    params: unknown,
  ): Protocol.Session.UnsubscribeParameters {
    if (params && typeof params === 'object' && 'subscriptions' in params) {
      return parseObject(
        params,
        WebDriverBidi.Session.UnsubscribeByIdRequestSchema,
      ) as Protocol.Session.UnsubscribeParameters;
    }
    return parseObject(
      params,
      WebDriverBidi.Session.UnsubscribeParametersSchema,
    ) as Protocol.Session.UnsubscribeParameters;
  }
}

export namespace Emulation {
  export function parseSetGeolocationOverrideParams(params: unknown) {
    if ('coordinates' in (params as object) && 'error' in (params as object)) {
      // Zod picks the first matching parameter omitting the other. In this case, the
      // `parseObject` will remove `error` from the params. However, specification
      // requires to throw an exception.
      throw new InvalidArgumentException(
        'Coordinates and error cannot be set at the same time',
      );
    }
    return parseObject(
      params,
      WebDriverBidi.Emulation.SetGeolocationOverrideParametersSchema,
    ) as Protocol.Emulation.SetGeolocationOverrideParameters;
  }
  export function parseSetLocaleOverrideParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Emulation.SetLocaleOverrideParametersSchema,
    ) as Protocol.Emulation.SetLocaleOverrideParameters;
  }
  export function parseSetScreenOrientationOverrideParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Emulation.SetScreenOrientationOverrideParametersSchema,
    ) as Protocol.Emulation.SetScreenOrientationOverrideParameters;
  }
  export function parseSetTimezoneOverrideParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Emulation.SetTimezoneOverrideParametersSchema,
    ) as Protocol.Emulation.SetTimezoneOverrideParameters;
  }
}

export namespace Input {
  export function parsePerformActionsParams(params: unknown) {
    return parseObject(
      params,
      WebDriverBidi.Input.PerformActionsParametersSchema,
    ) as Protocol.Input.PerformActionsParameters;
  }

  export function parseReleaseActionsParams(
    params: unknown,
  ): Protocol.Input.ReleaseActionsParameters {
    return parseObject(
      params,
      WebDriverBidi.Input.ReleaseActionsParametersSchema,
    );
  }

  export function parseSetFilesParams(
    params: unknown,
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
      WebDriverBidi.Storage.GetCookiesParametersSchema,
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
      WebDriverBidi.Storage.SetCookieParametersSchema,
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
      WebDriverBidi.Storage.DeleteCookiesParametersSchema,
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

  const ResolveRealmRequestSchema = z.object({
    realm: WebDriverBidi.Script.RealmSchema,
  });

  export function parseSendCommandRequest(
    params: unknown,
  ): Protocol.Cdp.SendCommandParameters {
    return parseObject(
      params,
      SendCommandRequestSchema,
    ) as Protocol.Cdp.SendCommandParameters;
  }

  export function parseGetSessionRequest(
    params: unknown,
  ): Protocol.Cdp.GetSessionParameters {
    return parseObject(params, GetSessionRequestSchema);
  }

  export function parseResolveRealmRequest(
    params: unknown,
  ): Protocol.Cdp.ResolveRealmParameters {
    return parseObject(params, ResolveRealmRequestSchema);
  }
}

export namespace Permissions {
  export function parseSetPermissionsParams(
    params: unknown,
  ): Protocol.Permissions.SetPermissionParameters {
    return {
      // TODO: remove once "goog:" attributes are not needed.
      ...(params as object),
      ...(parseObject(
        params,
        WebDriverBidiPermissions.Permissions.SetPermissionParametersSchema,
      ) as Protocol.Permissions.SetPermissionParameters),
    };
  }
}

export namespace Bluetooth {
  export function parseHandleRequestDevicePromptParams(
    params: unknown,
  ): Protocol.Bluetooth.HandleRequestDevicePromptParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth
        .HandleRequestDevicePromptParametersSchema,
    ) as Protocol.Bluetooth.HandleRequestDevicePromptParameters;
  }
  export function parseSimulateAdapterParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateAdapterParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth.SimulateAdapterParametersSchema,
    ) as Protocol.Bluetooth.SimulateAdapterParameters;
  }
  export function parseDisableSimulationParameters(
    params: unknown,
  ): Protocol.Bluetooth.DisableSimulationParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth.DisableSimulationParametersSchema,
    ) as Protocol.Bluetooth.DisableSimulationParameters;
  }
  export function parseSimulateAdvertisementParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateAdvertisementParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth.SimulateAdvertisementParametersSchema,
    ) as Protocol.Bluetooth.SimulateAdvertisementParameters;
  }
  export function parseSimulateCharacteristicParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateCharacteristicParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth.SimulateCharacteristicParametersSchema,
    ) as Protocol.Bluetooth.SimulateCharacteristicParameters;
  }
  export function parseSimulateCharacteristicResponseParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateCharacteristicResponseParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth
        .SimulateCharacteristicResponseParametersSchema,
    ) as Protocol.Bluetooth.SimulateCharacteristicResponseParameters;
  }
  export function parseSimulateDescriptorParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateDescriptorParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth.SimulateDescriptorParametersSchema,
    ) as Protocol.Bluetooth.SimulateDescriptorParameters;
  }
  export function parseSimulateDescriptorResponseParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateDescriptorResponseParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth
        .SimulateDescriptorResponseParametersSchema,
    ) as Protocol.Bluetooth.SimulateDescriptorResponseParameters;
  }
  export function parseSimulateGattConnectionResponseParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateGattConnectionResponseParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth
        .SimulateGattConnectionResponseParametersSchema,
    ) as Protocol.Bluetooth.SimulateGattConnectionResponseParameters;
  }
  export function parseSimulateGattDisconnectionParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateGattDisconnectionParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth
        .SimulateGattDisconnectionParametersSchema,
    ) as Protocol.Bluetooth.SimulateGattDisconnectionParameters;
  }
  export function parseSimulatePreconnectedPeripheralParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulatePreconnectedPeripheralParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth
        .SimulatePreconnectedPeripheralParametersSchema,
    ) as Protocol.Bluetooth.SimulatePreconnectedPeripheralParameters;
  }
  export function parseSimulateServiceParams(
    params: unknown,
  ): Protocol.Bluetooth.SimulateServiceParameters {
    return parseObject(
      params,
      WebDriverBidiBluetooth.Bluetooth.SimulateServiceParametersSchema,
    ) as Protocol.Bluetooth.SimulateServiceParameters;
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-webExtension */
export namespace WebModule {
  export function parseInstallParams(
    params: unknown,
  ): Protocol.WebExtension.InstallParameters {
    return parseObject(
      params,
      WebDriverBidi.WebExtension.InstallParametersSchema,
    );
  }
  export function parseUninstallParams(
    params: unknown,
  ): Protocol.WebExtension.UninstallParameters {
    return parseObject(
      params,
      WebDriverBidi.WebExtension.UninstallParametersSchema,
    );
  }
}
