// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Start point for Files app.
 */

import '../../common/js/error_counter.js';
import './metrics_start.js';

import {util} from '../../common/js/util.js';
import {FileManager} from './file_manager.js';


if (!window.isSWA) {
  console.warn(
      '%cYou are running Files chrome-app',
      'font-size: 2em; background-color: blue; color: white;');
}
const fileManager = new FileManager();
window.fileManager = fileManager;

fileManager.initializeCore();

fileManager.initializeUI(document.body)
    .then(() => util.testSendMessage('ready'));
