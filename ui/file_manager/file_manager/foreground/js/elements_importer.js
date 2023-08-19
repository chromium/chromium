// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getSanitizedScriptUrl} from '../../common/js/trusted_script_url_policy_util.js';

/** @return {!Promise<void>} */
export function importElements() {
  const startTime = Date.now();
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = getSanitizedScriptUrl('foreground/js/deferred_elements.js');

    script.onload = () => {
      console.log('Elements imported.');
      chrome.metricsPrivate.recordTime(
          'FileBrowser.Load.ImportElements', Date.now() - startTime);
      resolve();
    };
    script.onerror = (error) => {
      console.error(error);
      reject(error);
    };

    document.head.appendChild(script);
  });
}
