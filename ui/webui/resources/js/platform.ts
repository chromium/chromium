// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* @fileoverview Utilities for determining the current platform. */

/** Whether we are using a Mac or not. */
export const isMac = /Mac/.test(navigator.platform);

/** Whether this is on the Windows platform or not. */
export const isWindows = /Win/.test(navigator.platform);

/** Whether this is the ChromeOS/ash web browser. */
export const isChromeOS = (() => {
  let returnValue = false;
  // <if expr="chromeos_ash">
  returnValue = true;
  // </if>
  return returnValue;
})();

/** Whether this is the ChromeOS/Lacros web browser. */
export const isLacros = (() => {
  let returnValue = false;
  // <if expr="chromeos_lacros">
  returnValue = true;
  // </if>
  return returnValue;
})();

/** Whether this is on vanilla Linux (not chromeOS). */
export const isLinux = /Linux/.test(navigator.userAgent);

/** Whether this is on Android. */
export const isAndroid = /Android/.test(navigator.userAgent);

/** Whether this is on iOS. */
export const isIOS = /CriOS/.test(navigator.userAgent);
