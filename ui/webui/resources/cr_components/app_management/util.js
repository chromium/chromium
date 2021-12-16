// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';

import {AppManagementUserAction, AppType, OptionalBool} from './constants.js';
import {PermissionType} from './permission_constants.js';
import {isPermissionEnabled} from './permission_util.js';


/**
 * @fileoverview Utility functions for the App Management page.
 */

/**
 * @return {!AppManagementPageState}
 */
export function createEmptyState() {
  return {
    apps: {},
    selectedAppId: null,
  };
}

/**
 * @param {!Array<App>} apps
 * @return {!AppManagementPageState}
 */
export function createInitialState(apps) {
  const initialState = createEmptyState();

  for (const app of apps) {
    initialState.apps[app.id] = app;
  }

  return initialState;
}

/**
 * @param {App} app
 * @return {string}
 */
export function getAppIcon(app) {
  return `chrome://app-icon/${app.id}/64`;
}

/**
 * If the given value is not in the set, returns a new set with the value
 * added, otherwise returns the old set.
 * @template T
 * @param {!Set<T>} set
 * @param {T} value
 * @return {!Set<T>}
 */
export function addIfNeeded(set, value) {
  if (!set.has(value)) {
    set = new Set(set);
    set.add(value);
  }
  return set;
}

/**
 * If the given value is in the set, returns a new set without the value,
 * otherwise returns the old set.
 * @template T
 * @param {!Set<T>} set
 * @param {T} value
 * @return {!Set<T>}
 */
export function removeIfNeeded(set, value) {
  if (set.has(value)) {
    set = new Set(set);
    set.delete(value);
  }
  return set;
}

/**
 * @param {App} app
 * @param {string} permissionType
 * @return {boolean}
 */
export function getPermissionValueBool(app, permissionType) {
  const permission = getPermission(app, permissionType);
  assert(permission);

  return isPermissionEnabled(permission.value);
}

/**
 * Undefined is returned when the app does not request a permission.
 *
 * @param {App} app
 * @param {string} permissionType
 * @return {Permission|undefined}
 */
export function getPermission(app, permissionType) {
  return app.permissions[PermissionType[permissionType]];
}

/**
 * @param {AppManagementPageState} state
 * @return {?App}
 */
export function getSelectedApp(state) {
  const selectedAppId = state.selectedAppId;
  return selectedAppId ? state.apps[selectedAppId] : null;
}

/**
 * A comparator function to sort strings alphabetically.
 *
 * @param {string} a
 * @param {string} b
 */
export function alphabeticalSort(a, b) {
  return a.localeCompare(b);
}

/**
 * Toggles an OptionalBool
 *
 * @param {OptionalBool} bool
 * @return {OptionalBool}
 */
export function toggleOptionalBool(bool) {
  switch (bool) {
    case OptionalBool.kFalse:
      return OptionalBool.kTrue;
    case OptionalBool.kTrue:
      return OptionalBool.kFalse;
    default:
      assertNotReached();
  }
}

/**
 * @param {OptionalBool} optionalBool
 * @returns {boolean}
 */
export function convertOptionalBoolToBool(optionalBool) {
  switch (optionalBool) {
    case OptionalBool.kTrue:
      return true;
    case OptionalBool.kFalse:
      return false;
    default:
      assertNotReached();
  }
}

/**
 * @param {AppType} appType
 * @return {string}
 * @private
 */
export function getUserActionHistogramNameForAppType_(appType) {
  switch (appType) {
    case AppType.kArc:
      return 'AppManagement.AppDetailViews.ArcApp';
    case AppType.kChromeApp:
    case AppType.kStandaloneBrowser:
    case AppType.kStandaloneBrowserChromeApp:
      // TODO(https://crbug.com/1225848): Figure out appropriate behavior for
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

/**
 * @param {AppType} appType
 * @param {AppManagementUserAction} userAction
 */
export function recordAppManagementUserAction(appType, userAction) {
  const histogram = getUserActionHistogramNameForAppType_(appType);
  const enumLength = Object.keys(AppManagementUserAction).length;
  chrome.metricsPrivate.recordEnumerationValue(
      histogram, userAction, enumLength);
}
