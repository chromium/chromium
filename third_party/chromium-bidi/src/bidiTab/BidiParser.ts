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
import type {BidiCommandParameterParser} from '../bidiMapper/BidiMapper.js';
import type {
  Bluetooth,
  Browser,
  BrowsingContext,
  Cdp,
  Emulation,
  Input,
  Network,
  Permissions,
  Script,
  Session,
  Storage,
  WebExtension,
} from '../protocol/protocol.js';
import * as Parser from '../protocol-parser/protocol-parser.js';

export class BidiParser implements BidiCommandParameterParser {
  // Bluetooth module
  // keep-sorted start block=yes
  parseDisableSimulationParameters(
    params: unknown,
  ): Bluetooth.DisableSimulationParameters {
    return Parser.Bluetooth.parseDisableSimulationParameters(params);
  }
  parseHandleRequestDevicePromptParams(
    params: unknown,
  ): Bluetooth.HandleRequestDevicePromptParameters {
    return Parser.Bluetooth.parseHandleRequestDevicePromptParams(params);
  }
  parseSimulateAdapterParameters(
    params: unknown,
  ): Bluetooth.SimulateAdapterParameters {
    return Parser.Bluetooth.parseSimulateAdapterParams(params);
  }
  parseSimulateAdvertisementParameters(
    params: unknown,
  ): Bluetooth.SimulateAdvertisementParameters {
    return Parser.Bluetooth.parseSimulateAdvertisementParams(params);
  }
  parseSimulateCharacteristicParameters(
    params: unknown,
  ): Bluetooth.SimulateCharacteristicParameters {
    return Parser.Bluetooth.parseSimulateCharacteristicParams(params);
  }
  parseSimulateCharacteristicResponseParameters(
    params: unknown,
  ): Bluetooth.SimulateCharacteristicResponseParameters {
    return Parser.Bluetooth.parseSimulateCharacteristicResponseParams(params);
  }
  parseSimulateDescriptorParameters(
    params: unknown,
  ): Bluetooth.SimulateDescriptorParameters {
    return Parser.Bluetooth.parseSimulateDescriptorParams(params);
  }
  parseSimulateDescriptorResponseParameters(
    params: unknown,
  ): Bluetooth.SimulateDescriptorResponseParameters {
    return Parser.Bluetooth.parseSimulateDescriptorResponseParams(params);
  }
  parseSimulateGattConnectionResponseParameters(
    params: unknown,
  ): Bluetooth.SimulateGattConnectionResponseParameters {
    return Parser.Bluetooth.parseSimulateGattConnectionResponseParams(params);
  }
  parseSimulateGattDisconnectionParameters(
    params: unknown,
  ): Bluetooth.SimulateGattDisconnectionParameters {
    return Parser.Bluetooth.parseSimulateGattDisconnectionParams(params);
  }
  parseSimulatePreconnectedPeripheralParameters(
    params: unknown,
  ): Bluetooth.SimulatePreconnectedPeripheralParameters {
    return Parser.Bluetooth.parseSimulatePreconnectedPeripheralParams(params);
  }
  parseSimulateServiceParameters(
    params: unknown,
  ): Bluetooth.SimulateServiceParameters {
    return Parser.Bluetooth.parseSimulateServiceParams(params);
  }
  // keep-sorted end

  // Browser module
  // keep-sorted start block=yes
  parseCreateUserContextParameters(
    params: unknown,
  ): Browser.CreateUserContextParameters {
    // Validate the params, but return the original one, as there can be `goog:` options.
    Parser.Browser.parseCreateUserContextParameters(params);
    return params as Browser.CreateUserContextParameters;
  }
  parseRemoveUserContextParameters(
    params: unknown,
  ): Browser.RemoveUserContextParameters {
    return Parser.Browser.parseRemoveUserContextParameters(params);
  }
  parseSetClientWindowStateParameters(
    params: unknown,
  ): Browser.SetClientWindowStateParameters {
    return Parser.Browser.parseSetClientWindowStateParameters(params);
  }
  // keep-sorted end

  // Browsing Context module
  // keep-sorted start block=yes
  parseActivateParams(params: unknown): BrowsingContext.ActivateParameters {
    return Parser.BrowsingContext.parseActivateParams(params);
  }
  parseCaptureScreenshotParams(
    params: unknown,
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
    params: unknown,
  ): BrowsingContext.HandleUserPromptParameters {
    return Parser.BrowsingContext.parseHandleUserPromptParameters(params);
  }
  parseLocateNodesParams(
    params: unknown,
  ): BrowsingContext.LocateNodesParameters {
    return Parser.BrowsingContext.parseLocateNodesParams(params);
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
    params: unknown,
  ): BrowsingContext.SetViewportParameters {
    return Parser.BrowsingContext.parseSetViewportParams(params);
  }
  parseTraverseHistoryParams(
    params: unknown,
  ): BrowsingContext.TraverseHistoryParameters {
    return Parser.BrowsingContext.parseTraverseHistoryParams(params);
  }
  // keep-sorted end

  // CDP module
  // keep-sorted start block=yes
  parseGetSessionParams(params: unknown): Cdp.GetSessionParameters {
    return Parser.Cdp.parseGetSessionRequest(params);
  }
  parseResolveRealmParams(params: unknown): Cdp.ResolveRealmParameters {
    return Parser.Cdp.parseResolveRealmRequest(params);
  }
  parseSendCommandParams(params: unknown): Cdp.SendCommandParameters {
    return Parser.Cdp.parseSendCommandRequest(params);
  }
  // keep-sorted end

  // Emulation module
  // keep-sorted start block=yes
  parseSetGeolocationOverrideParams(
    params: unknown,
  ): Emulation.SetGeolocationOverrideParameters {
    return Parser.Emulation.parseSetGeolocationOverrideParams(params);
  }
  parseSetLocaleOverrideParams(
    params: unknown,
  ): Emulation.SetLocaleOverrideParameters {
    return Parser.Emulation.parseSetLocaleOverrideParams(params);
  }
  parseSetScreenOrientationOverrideParams(
    params: unknown,
  ): Emulation.SetScreenOrientationOverrideParameters {
    return Parser.Emulation.parseSetScreenOrientationOverrideParams(params);
  }
  parseSetTimezoneOverrideParams(
    params: unknown,
  ): Emulation.SetTimezoneOverrideParameters {
    return Parser.Emulation.parseSetTimezoneOverrideParams(params);
  }
  // keep-sorted end

  // Input module
  // keep-sorted start block=yes
  parsePerformActionsParams(params: unknown): Input.PerformActionsParameters {
    return Parser.Input.parsePerformActionsParams(params);
  }
  parseReleaseActionsParams(params: unknown): Input.ReleaseActionsParameters {
    return Parser.Input.parseReleaseActionsParams(params);
  }
  parseSetFilesParams(params: unknown): Input.SetFilesParameters {
    return Parser.Input.parseSetFilesParams(params);
  }
  // keep-sorted end

  // Network module
  // keep-sorted start block=yes
  parseAddDataCollectorParams(
    params: unknown,
  ): Network.AddDataCollectorParameters {
    return Parser.Network.parseAddDataCollectorParameters(params);
  }
  parseAddInterceptParams(params: unknown): Network.AddInterceptParameters {
    return Parser.Network.parseAddInterceptParameters(params);
  }
  parseContinueRequestParams(
    params: unknown,
  ): Network.ContinueRequestParameters {
    return Parser.Network.parseContinueRequestParameters(params);
  }
  parseContinueResponseParams(
    params: unknown,
  ): Network.ContinueResponseParameters {
    return Parser.Network.parseContinueResponseParameters(params);
  }
  parseContinueWithAuthParams(
    params: unknown,
  ): Network.ContinueWithAuthParameters {
    return Parser.Network.parseContinueWithAuthParameters(params);
  }
  parseDisownDataParams(params: unknown): Network.DisownDataParameters {
    return Parser.Network.parseDisownDataParameters(params);
  }
  parseFailRequestParams(params: unknown): Network.FailRequestParameters {
    return Parser.Network.parseFailRequestParameters(params);
  }
  parseGetDataParams(params: unknown): Network.GetDataParameters {
    return Parser.Network.parseGetDataParameters(params);
  }
  parseProvideResponseParams(
    params: unknown,
  ): Network.ProvideResponseParameters {
    return Parser.Network.parseProvideResponseParameters(params);
  }
  parseRemoveDataCollectorParams(
    params: unknown,
  ): Network.RemoveDataCollectorParameters {
    return Parser.Network.parseRemoveDataCollectorParameters(params);
  }
  parseRemoveInterceptParams(
    params: unknown,
  ): Network.RemoveInterceptParameters {
    return Parser.Network.parseRemoveInterceptParameters(params);
  }
  parseSetCacheBehavior(params: unknown): Network.SetCacheBehaviorParameters {
    return Parser.Network.parseSetCacheBehavior(params);
  }
  // keep-sorted end

  // Permissions module
  // keep-sorted start block=yes
  parseSetPermissionsParams(
    params: unknown,
  ): Permissions.SetPermissionParameters {
    return Parser.Permissions.parseSetPermissionsParams(params);
  }
  // keep-sorted end

  // Script module
  // keep-sorted start block=yes
  parseAddPreloadScriptParams(
    params: unknown,
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
    params: unknown,
  ): Script.RemovePreloadScriptParameters {
    return Parser.Script.parseRemovePreloadScriptParams(params);
  }
  // keep-sorted end

  // Session module
  // keep-sorted start block=yes
  parseSubscribeParams(params: unknown): Session.SubscriptionRequest {
    return Parser.Session.parseSubscribeParams(params);
  }
  parseUnsubscribeParams(params: unknown): Session.UnsubscribeParameters {
    return Parser.Session.parseUnsubscribeParams(params);
  }
  // keep-sorted end

  // Storage module
  // keep-sorted start block=yes
  parseDeleteCookiesParams(params: unknown): Storage.DeleteCookiesParameters {
    return Parser.Storage.parseDeleteCookiesParams(params);
  }
  parseGetCookiesParams(params: unknown): Storage.GetCookiesParameters {
    return Parser.Storage.parseGetCookiesParams(params);
  }
  parseSetCookieParams(params: unknown): Storage.SetCookieParameters {
    return Parser.Storage.parseSetCookieParams(params);
  }
  // keep-sorted end

  // WebExtenstion module
  // keep-sorted start block=yes
  parseInstallParams(params: unknown): WebExtension.InstallParameters {
    return Parser.WebModule.parseInstallParams(params);
  }
  parseUninstallParams(params: unknown): WebExtension.UninstallParameters {
    return Parser.WebModule.parseUninstallParams(params);
  }
  // keep-sorted end
}
