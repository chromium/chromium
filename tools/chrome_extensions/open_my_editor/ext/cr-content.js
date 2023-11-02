// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For codereview.chromium.org

let clicked_element = null;

document.addEventListener('contextmenu', (event) => {
  clicked_element = event.target;
});

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request == 'getFiles') {
    let element = clicked_element;
    clicked_element = null;
    while (element != null && element.tagName != 'TABLE')
      element = element.parentElement;

    let trs = element.getElementsByTagName('TR');
    if (trs.length == 0)
      alert('Please toggle one patchset.');

    // TODO(watk): Sometimes this approach collects duplicates, but I'm not
    // sure of the conditions under which it happens, so use a Set for now.
    let files = new Set();
    for (let i = 1; i < trs.length; ++i) {
      let tr = trs[i];
      if (tr.getAttribute('name') != 'patch')
        continue;
      // Skip deleted file.
      if (tr.children[1].firstChild.data == 'D')
        continue;

      files.add(tr.children[2].children[0].text.replace(/\s*/g, ''));
    }

    sendResponse({files: Array.from(files)});
  } else if (request == 'getFile' && clicked_element.tagName == 'A') {
    let filepath = clicked_element.text.replace(/\s*/g, '');
    clicked_element = null;
    sendResponse({file: filepath});
  }
});
