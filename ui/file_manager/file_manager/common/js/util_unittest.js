// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {LEGACY_FILES_APP_URL, SWA_FILES_APP_URL} from './url_constants.js';
import {util} from './util.js';

export function testExtractFilePath() {
  let url = '';

  assertEquals(util.extractFilePath(''), null);
  assertEquals(util.extractFilePath(null), null);
  assertEquals(util.extractFilePath(undefined), null);

  // In the Extension:
  const zipPath = '/Downloads-u/Downloads/f.zip';
  url = `filesystem:${LEGACY_FILES_APP_URL}external${zipPath}`;
  assertEquals(util.extractFilePath(url), zipPath);

  // In the SWA:
  url = `filesystem:${SWA_FILES_APP_URL}external${zipPath}`;
  assertEquals(util.extractFilePath(url), zipPath);
}
