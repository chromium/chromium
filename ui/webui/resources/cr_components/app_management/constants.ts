// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {App} from './app_management.mojom-webui.js';

export {AppType, InstallReason, InstallSource, RunOnOsLogin, RunOnOsLoginMode, WindowMode} from './app_management.mojom-webui.js';

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
  APP_LIST_CONTEXT_MENU_APP_INFO_ARC = 0,
  APP_LIST_CONTEXT_MENU_APP_INFO_CHROME_APP = 1,
  APP_LIST_CONTEXT_MENU_APP_INFO_WEB_APP = 2,
  SHELF_CONTEXT_MENU_APP_INFO_ARC = 3,
  SHELF_CONTEXT_MENU_APP_INFO_CHROME_APP = 4,
  SHELF_CONTEXT_MENU_APP_INFO_WEB_APP = 5,
  MAIN_VIEW_ARC = 6,
  MAIN_VIEW_CHROME_APP = 7,
  MAIN_VIEW_WEB_APP = 8,
  OS_SETTINGS_MAIN_PAGE = 9,
  MAIN_VIEW_PLUGIN_VM = 10,
  D_BUS_SERVICE_PLUGIN_VM = 11,
  MAIN_VIEW_BOREALIS = 12,
}

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum AppManagementUserAction {
  VIEW_OPENED = 0,
  NATIVE_SETTINGS_OPENED = 1,
  UNINSTALL_DIALOG_LAUNCHED = 2,
  PIN_TO_SHELF_TURNED_ON = 3,
  PIN_TO_SHELF_TURNED_OFF = 4,
  NOTIFICATIONS_TURNED_ON = 5,
  NOTIFICATIONS_TURNED_OFF = 6,
  LOCATION_TURNED_ON = 7,
  LOCATION_TURNED_OFF = 8,
  CAMERA_TURNED_ON = 9,
  CAMERA_TURNED_OFF = 10,
  MICROPHONE_TURNED_ON = 11,
  MICROPHONE_TURNED_OFF = 12,
  CONTACTS_TURNED_ON = 13,
  CONTACTS_TURNED_OFF = 14,
  STORAGE_TURNED_ON = 15,
  STORAGE_TURNED_OFF = 16,
  PRINTING_TURNED_ON = 17,
  PRINTING_TURNED_OFF = 18,
  RESIZE_LOCK_TURNED_ON = 19,
  RESIZE_LOCK_TURNED_OFF = 20,
  PREFERRED_APP_TURNED_ON = 21,
  PREFERRED_APP_TURNED_OFF = 22,
  SUPPORTED_LINKS_LIST_SHOWN = 23,
  OVERLAPPING_APPS_DIALOG_SHOWN = 24,
  WINDOW_MODE_CHANGED_TO_BROWSER = 25,
  WINDOW_MODE_CHANGED_TO_WINDOW = 26,
  RUN_ON_OS_LOGIN_MODE_TURNED_ON = 27,
  RUN_ON_OS_LOGIN_MODE_TURNED_OFF = 28,
  FILE_HANDLING_TURNED_ON = 29,
  FILE_HANDLING_TURNED_OFF = 30,
  FILE_HANDLING_OVERFLOW_SHOWN = 31,
  APP_STORE_LINK_CLICKED = 32,
}

// A Record (tuple) of app IDs to app used mostly for the supported links
// frontend components.
export type AppMap = Record<string, App>;
