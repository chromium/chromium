// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adds the loadTimeData values that depend on the profile of the window that
// shows a Top Chrome WebUI. In TopChromeWebUIProfile, the WebUIDataSource is
// shared by the windows of all profiles, so TopChromeWebUIController passes
// these values to each frame instead (see AddProfileLoadTimeData()). In other
// profiles, nothing is passed, and chrome.getVariableValue() may not be
// available. Entry points must import '/strings.m.js' and then this module
// before the elements that read the values.
import {loadTimeData} from './load_time_data.js';

const profileLoadTimeData = chrome.getVariableValue?.('profileLoadTimeData');
if (profileLoadTimeData) {
  loadTimeData.overrideValues(JSON.parse(profileLoadTimeData));
}
