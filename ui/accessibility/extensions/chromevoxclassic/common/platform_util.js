// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for working with platforms.
 */


goog.provide('cvox.PlatformFilter');
goog.provide('cvox.PlatformUtil');

goog.require('cvox.ChromeVox');

/**
 * @enum
 */
cvox.PlatformFilter = {
  NONE: 0,
  WINDOWS: 1,
  MAC: 2,
  LINUX: 4,
  WML: 7,
  CHROMEOS: 8,
  ANDROID: 16
};


/**
 *Checks whether the given filter matches the current platform. An undefined
 * filter always matches the current platform.
 * @param {undefined|cvox.PlatformFilter|number} filter The filter.
 * @return {boolean} Whether the filter matches the current platform.
 */
cvox.PlatformUtil.matchesPlatform = function(filter) {
  var uA = navigator.userAgent;
  if (filter == undefined) {
    return true;
  } else if (uA.indexOf('Android') != -1) {
    return (filter & cvox.PlatformFilter.ANDROID) != 0;
  } else if (uA.indexOf('Win') != -1) {
    return (filter & cvox.PlatformFilter.WINDOWS) != 0;
  } else if (uA.indexOf('Mac') != -1) {
    return (filter & cvox.PlatformFilter.MAC) != 0;
  } else if (uA.indexOf('Linux') != -1) {
    return (filter & cvox.PlatformFilter.LINUX) != 0;
  } else if (uA.indexOf('CrOS') != -1) {
    return (filter & cvox.PlatformFilter.CHROMEOS) != 0;
  }
  return false;
};
