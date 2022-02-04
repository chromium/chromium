// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export {AppType, InstallReason, OptionalBool, RunOnOsLogin, RunOnOsLoginMode, WindowMode} from './types.mojom-webui.js';

/**
 * The number of apps displayed in app list in the main view before expanding.
 */
export const NUMBER_OF_APPS_DISPLAYED_DEFAULT = 4;

// Enumeration of the different subpage types within the app management page.
export enum PageType {
  MAIN = 0,
  DETAIL = 1,
}

// This histogram is also declared and used at chrome/browser/ui/webui/settings/
// chromeos/app_management/app_management_uma.h.
export const AppManagementEntryPointsHistogramName =
    'AppManagement.EntryPoints';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum AppManagementEntryPoint {
  AppListContextMenuAppInfoArc = 0,
  AppListContextMenuAppInfoChromeApp = 1,
  AppListContextMenuAppInfoWebApp = 2,
  ShelfContextMenuAppInfoArc = 3,
  ShelfContextMenuAppInfoChromeApp = 4,
  ShelfContextMenuAppInfoWebApp = 5,
  MainViewArc = 6,
  MainViewChromeApp = 7,
  MainViewWebApp = 8,
  OsSettingsMainPage = 9,
  MainViewPluginVm = 10,
  DBusServicePluginVm = 11,
  MainViewBorealis = 12,
}

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum AppManagementUserAction {
  ViewOpened = 0,
  NativeSettingsOpened = 1,
  UninstallDialogLaunched = 2,
  PinToShelfTurnedOn = 3,
  PinToShelfTurnedOff = 4,
  NotificationsTurnedOn = 5,
  NotificationsTurnedOff = 6,
  LocationTurnedOn = 7,
  LocationTurnedOff = 8,
  CameraTurnedOn = 9,
  CameraTurnedOff = 10,
  MicrophoneTurnedOn = 11,
  MicrophoneTurnedOff = 12,
  ContactsTurnedOn = 13,
  ContactsTurnedOff = 14,
  StorageTurnedOn = 15,
  StorageTurnedOff = 16,
  PrintingTurnedOn = 17,
  PrintingTurnedOff = 18,
  ResizeLockTurnedOn = 19,
  ResizeLockTurnedOff = 20,
  PreferredAppTurnedOn = 21,
  PreferredAppTurnedOff = 22,
  SupportedLinksListShown = 23,
  OverlappingAppsDialogShown = 24,
  WindowModeChangedToBrowser = 25,
  WindowModeChangedToWindow = 26,
  RunOnOsLoginModeTurnedOn = 27,
  RunOnOsLoginModeTurnedOff = 28,
}
