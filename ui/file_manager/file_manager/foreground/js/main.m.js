// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Start point for Files app.
 */

import '../../../base/js/error_counter.m.js';
import './metrics_start.m.js';

import {util} from '../../common/js/util.m.js';
import {FileManager} from './file_manager.m.js';


const fileManager = new FileManager();
window.fileManager = fileManager;

fileManager.initializeCore();

fileManager.initializeUI(document.body)
    .then(() => util.testSendMessage('ready'));
