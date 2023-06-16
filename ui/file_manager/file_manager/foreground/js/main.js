// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Start point for Files app.
 */

import '../../common/js/error_counter.js';
import '../../widgets/xf_jellybean.js';
import './metrics_start.js';
import 'chrome://resources/cros_components/switch/switch.js';

import {util} from '../../common/js/util.js';

import {FileManager} from './file_manager.js';


const fileManager = new FileManager();
window.fileManager = fileManager;

fileManager.initializeCore();

fileManager.initializeUI(document.body).then(() => {
  util.testSendMessage('ready');
});
