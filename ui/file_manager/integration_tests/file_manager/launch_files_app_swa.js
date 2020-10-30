// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Tests that files app SWA launches.
   */
  testcase.launchFilesAppSwa = async () => {
    const caller = getCaller();

    const swaAppId = await sendTestMessage({
      name: 'launchFileManagerSwa',
    });

    console.log('file_manager_swa_id: ' + swaAppId);

    await repeatUntil(async () => {
      const launched = await sendTestMessage({
        name: 'hasSwaStarted',
        swaAppId: swaAppId,
      });

      if (launched !== 'true') {
        return pending(caller, 'Waiting for files app SWA launch');
      }
    });

    return IGNORE_APP_ERRORS;
  };
})();
