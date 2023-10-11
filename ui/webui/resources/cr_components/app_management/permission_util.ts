// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';

import {Permission, PermissionType, PermissionValue, TriState} from './app_management.mojom-webui.js';

export function createPermission(
    permissionType: PermissionType, value: PermissionValue,
    isManaged: boolean): Permission {
  return {
    permissionType,
    value,
    isManaged,
  };
}

export function createTriStatePermissionValue(value: TriState):
    PermissionValue {
  return {tristateValue: value} as PermissionValue;
}

export function getTriStatePermissionValue(permissionValue: PermissionValue):
    TriState {
  assert(isTriStateValue(permissionValue));
  return permissionValue.tristateValue!;
}

export function createBoolPermissionValue(value: boolean): PermissionValue {
  return {boolValue: value} as PermissionValue;
}

export function getBoolPermissionValue(permissionValue: PermissionValue):
    boolean {
  assert(isBoolValue(permissionValue));
  return permissionValue.boolValue!;
}

export function isTriStateValue(permissionValue: PermissionValue): boolean {
  return permissionValue['tristateValue'] !== undefined &&
      permissionValue['boolValue'] === undefined;
}

export function isBoolValue(permissionValue: PermissionValue): boolean {
  return permissionValue['boolValue'] !== undefined &&
      permissionValue['tristateValue'] === undefined;
}

export function createBoolPermission(
    permissionType: PermissionType, value: boolean,
    isManaged: boolean): Permission {
  return createPermission(
      permissionType, createBoolPermissionValue(value), isManaged);
}

export function createTriStatePermission(
    permissionType: PermissionType, value: TriState,
    isManaged: boolean): Permission {
  return createPermission(
      permissionType, createTriStatePermissionValue(value), isManaged);
}

export function isPermissionEnabled(permissionValue: PermissionValue): boolean {
  if (isBoolValue(permissionValue)) {
    return getBoolPermissionValue(permissionValue)!;
  }

  if (isTriStateValue(permissionValue)) {
    return getTriStatePermissionValue(permissionValue) === TriState.kAllow;
  }

  assertNotReached();
}
