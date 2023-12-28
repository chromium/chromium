// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Start point for Files app.
 */

import '../../background/js/metrics_start.js';
import '../../common/js/error_counter.js';
import './metrics_start.js';
import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {GlitchType, reportGlitch} from '../../common/js/glitch.js';
import {testSendMessage} from '../../common/js/util.js';

import {FileManager} from './file_manager.js';


const fileManager = new FileManager();
window.fileManager = fileManager;

async function run() {
  try {
    window.appID = String(loadTimeData.getInteger('WINDOW_NUMBER'));
  } catch (e) {
    reportGlitch(GlitchType.CAUGHT_EXCEPTION);
    console.warn('Failed to get the app ID', e);
  }
}

run().then(() => {
  fileManager.initializeCore();

  fileManager.initializeUi(document.body).then(() => {
    testSendMessage('ready');
  });
});
