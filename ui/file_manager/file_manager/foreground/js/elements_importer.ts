// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ScriptLoader} from '../../common/js/script_loader.js';

export async function importElements(): Promise<void> {
  const scriptLoader =
      new ScriptLoader('foreground/js/deferred_elements.js', {type: 'module'});
  const startTime = Date.now();
  try {
    await scriptLoader.load();
    console.info('Elements imported.');
    chrome.metricsPrivate.recordTime(
        'FileBrowser.Load.ImportElements', Date.now() - startTime);
  } catch (error) {
    console.error(error);
  }
}
