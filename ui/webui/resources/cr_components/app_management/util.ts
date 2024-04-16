// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';

import type {App, Permission} from './app_management.mojom-webui.js';
import {PermissionType, TriState} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction, AppType} from './constants.js';
import type {PermissionTypeIndex} from './permission_constants.js';
import {isBoolValue, isPermissionEnabled, isTriStateValue} from './permission_util.js';

/**
 * @fileoverview Utility functions for the App Management page.
 */

interface AppManagementPageState {
  apps: Record<string, App>;
  selectedAppId: string|null;
  // Maps all apps to their parent's app ID. Apps without a parent are
  // not listed in this map.
  subAppToParentAppId: Record<string, string>;
}

export function createEmptyState(): AppManagementPageState {
  return {
    apps: {},
    selectedAppId: null,
    subAppToParentAppId: {},
  };
}

export function createInitialState(
    apps: App[],
    subAppToParentAppId: {[key: string]: string}): AppManagementPageState {
  const initialState = createEmptyState();

  for (const app of apps) {
    initialState.apps[app.id] = app;
  }

  initialState.subAppToParentAppId = subAppToParentAppId;

  return initialState;
}

export function getAppIcon(app: App): string {
  return `chrome://app-icon/${app.id}/64`;
}

export function getPermissionValueBool(
    app: App, permissionType: PermissionTypeIndex): boolean {
  const permission = getPermission(app, permissionType);
  assert(permission);

  return isPermissionEnabled(permission.value);
}

/**
 * Returns the TriState value of a permission. If the permission value is not
 * already a TriState, it will be converted based on the boolean value.
 */
export function getPermissionValueAsTriState(
    app: App, permissionType: PermissionTypeIndex): TriState {
  const permission = getPermission(app, permissionType);
  assert(permission);

  if (isTriStateValue(permission.value)) {
    return permission.value.tristateValue!;
  }

  if (isBoolValue(permission.value)) {
    return permission.value.boolValue!? TriState.kAllow : TriState.kBlock;
  }

  assertNotReached();
}

/**
 * Undefined is returned when the app does not request a permission.
 */
export function getPermission(
    app: App, permissionType: PermissionTypeIndex): Permission|undefined {
  return app.permissions[PermissionType[permissionType]];
}

export function getSelectedApp(state: AppManagementPageState): App|null {
  const selectedAppId = state.selectedAppId;
  return selectedAppId ? state.apps[selectedAppId]! : null;
}

/**
 * Returns a list of all apps whose parent's app ID matches the selected app.
 */
export function getSubAppsOfSelectedApp(state: AppManagementPageState): App[] {
  const selectedAppId = state.selectedAppId;
  const result = selectedAppId ?
      Object.values(state.apps)
          .filter(
              (app) => state.subAppToParentAppId[app.id] === selectedAppId) :
      [];
  return result;
}

/**
 * Returns the selected app's parent app or null.
 */
export function getParentApp(state: AppManagementPageState): App|null {
  const selectedAppId = state.selectedAppId;
  if (selectedAppId) {
    const parentAppId = state.subAppToParentAppId[selectedAppId];
    return parentAppId ? state.apps[parentAppId]! : null;
  }
  return null;
}

/**
 * A comparator function to sort strings alphabetically.
 */
export function alphabeticalSort(a: string, b: string) {
  return a.localeCompare(b);
}

function getUserActionHistogramNameForAppType(appType: AppType): string {
  switch (appType) {
    case AppType.kArc:
      return 'AppManagement.AppDetailViews.ArcApp';
    case AppType.kChromeApp:
    case AppType.kStandaloneBrowser:
    case AppType.kStandaloneBrowserChromeApp:
      // TODO(crbug.com/40188614): Figure out appropriate behavior for
      // Lacros-hosted chrome-apps.
      return 'AppManagement.AppDetailViews.ChromeApp';
    case AppType.kWeb:
      return 'AppManagement.AppDetailViews.WebApp';
    case AppType.kPluginVm:
      return 'AppManagement.AppDetailViews.PluginVmApp';
    case AppType.kBorealis:
      return 'AppManagement.AppDetailViews.BorealisApp';
    default:
      assertNotReached();
  }
}

export function recordAppManagementUserAction(
    appType: AppType, userAction: AppManagementUserAction) {
  const histogram = getUserActionHistogramNameForAppType(appType);
  const enumLength = Object.keys(AppManagementUserAction).length;
  BrowserProxy.getInstance().recordEnumerationValue(
      histogram, userAction, enumLength);
}

/**
 * @param arg An argument to check for existence.
 * @throws If |arg| is undefined or null.
 */
export function assertExists<T>(
    arg: T, message: string = `Expected ${arg} to be defined.`):
    asserts arg is NonNullable<T> {
  assert(arg !== undefined && arg !== null, message);
}

/**
 * @param arg A argument to check for existence.
 * @return |arg| with the type narrowed as non-nullable.
 * @throws If |arg| is undefined or null.
 */
export function castExists<T>(arg: T, message?: string): NonNullable<T> {
  assertExists(arg, message);
  return arg;
}
