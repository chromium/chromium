// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes}
 */

import {CHOOSE_ENTRY_PROPERTY, NO_ENTRIES_CHOSEN} from './choose_entry_const.js';

/**
 * Extracts parameters used by chooseEntry function.
 */
function getDialogParams(): chrome.fileSystem.ChooseEntryOptions {
  const queryStr = window.location.search;
  const params = new URLSearchParams(queryStr);
  return JSON.parse(params.get('value')!);
}

// Send a "clear previous results" message, call chooseEntry and send the
// result (as a message) to the main page.
chrome.runtime.sendMessage(null, {msgType: CHOOSE_ENTRY_PROPERTY}, undefined);
const params: chrome.fileSystem.ChooseEntryOptions = getDialogParams();
chrome.fileSystem.chooseEntry(params, (entry?: FileSystemFileEntry) => {
  if (chrome.runtime.lastError &&
      (chrome.runtime.lastError.message !== 'User cancelled')) {
    console.error(chrome.runtime.lastError);
  }

  // entry's TS type is (FileSystemFileEntry | undefined) but that is a lie.
  //
  // https://source.chromium.org/chromium/chromium/src/+/main:tools/typescript/definitions/file_system.d.ts;l=37-42;drc=0345221adefeb4f5f77cb57dd2dd1e8159b30762
  //
  // There's also this declaration, which only confuses things.
  //
  // https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/api/file_system.idl;l=137-138;drc=0345221adefeb4f5f77cb57dd2dd1e8159b30762
  //
  // https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/api/file_system.idl;l=102-104;drc=0345221adefeb4f5f77cb57dd2dd1e8159b30762
  //
  // See also crbug.com/1313625
  //
  // In practice, it's (Entry | Entry[] | undefined).
  let entryNames: string = NO_ENTRIES_CHOSEN;
  if (!entry) {
    // No-op.
  } else if (params.acceptsMultiple) {
    const actualEntries: Entry[] = entry as unknown as Entry[];
    entryNames = actualEntries.map(entry => entry.name).sort().toString();
  } else {
    const actualEntry: Entry = entry as unknown as Entry;
    entryNames = actualEntry.name;
  }

  chrome.runtime.sendMessage(
      null, {msgType: CHOOSE_ENTRY_PROPERTY, entryNames: entryNames},
      undefined);
});
