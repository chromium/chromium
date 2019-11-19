// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var volumeManagerFactory = {};

/**
 * @return {!Promise<!VolumeManager>}
 */
volumeManagerFactory.getInstance = function() {};

/**
 * @return {VolumeManager}
 */
volumeManagerFactory.getInstanceForDebug = function() {};

volumeManagerFactory.revokeInstanceForTesting = function() {};
