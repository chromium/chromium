// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {test} from '../../file_manager/background/js/test_util_base.js';

import {open} from './background.js';

/**
 * Opens the audio player and waits until it is ready.
 *
 * @param {!Array<string>} urls URLs to be opened.
 *
 */
test.util.async.openAudioPlayer = function(urls, callback) {
  open(urls).then(callback);
};

// Register the test utils.
test.util.registerRemoteTestUtils('/js/runtime_loaded_test_util.js');
