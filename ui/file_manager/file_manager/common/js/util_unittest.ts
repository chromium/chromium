// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {LEGACY_FILES_APP_URL, SWA_FILES_APP_URL} from './url_constants.js';
import {extractFilePath} from './util.js';

export function testExtractFilePath() {
  let url = '';

  assertEquals(extractFilePath(''), null);
  assertEquals(extractFilePath(null), null);
  assertEquals(extractFilePath(undefined), null);

  // In the Extension:
  const zipPath = '/Downloads-u/Downloads/f.zip';
  url = `filesystem:${LEGACY_FILES_APP_URL}external${zipPath}`;
  assertEquals(extractFilePath(url), zipPath);

  // In the SWA:
  url = `filesystem:${SWA_FILES_APP_URL}external${zipPath}`;
  assertEquals(extractFilePath(url), zipPath);
}
