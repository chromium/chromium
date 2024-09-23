// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Strings required for policy indicators. These must be set at runtime.
 * Chrome OS only strings may be undefined.
 */
export interface CrPolicyStringsType {
  controlledSettingExtension: string;
  controlledSettingExtensionWithoutName: string;
  controlledSettingPolicy: string;
  controlledSettingRecommendedMatches: string;
  controlledSettingRecommendedDiffers: string;
  controlledSettingParent: string;
  controlledSettingChildRestriction: string;

  // <if expr="chromeos_ash">
  controlledSettingShared: string;
  controlledSettingWithOwner: string;
  controlledSettingNoOwner: string;
  // </if>
}

declare global {
  interface Window {
    CrPolicyStrings: Partial<CrPolicyStringsType>;
  }
}

/**
 * Possible policy indicators that can be shown in settings.
 */
export enum CrPolicyIndicatorType {
  DEVICE_POLICY = 'devicePolicy',
  EXTENSION = 'extension',
  NONE = 'none',
  OWNER = 'owner',
  PRIMARY_USER = 'primary_user',
  RECOMMENDED = 'recommended',
  USER_POLICY = 'userPolicy',
  PARENT = 'parent',
  CHILD_RESTRICTION = 'childRestriction',
}
