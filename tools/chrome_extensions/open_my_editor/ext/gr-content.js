// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OME for the chromium gerrit codereview site.

let clicked_element = null;

document.addEventListener('contextmenu', (event) => {
  clicked_element = event.target;
});

function handleGerritFilePaths(request, sendResponse) {
  let files = [];

  let elements = document.querySelectorAll('.file-row');
  for (var i = 1; i < elements.length; ++i) {
    let path = elements[i].getAttribute('data-path');
    if (!path || !path.length)
      continue;  // Empty file path in the file list: ignore.
    let stat = elements[i].querySelector('.status');
    if (!stat.textContent || stat.textContent.trim() == 'D')
      continue;  // File path has status D (deleted): ignore.
    files.push(path);
  }

  sendResponse({files: files});
}

function handleGerritFile(request, sendResponse) {
  let file = clicked_element.getAttribute('title');
  if (!file || !file.length)
    return;
  if (file.indexOf('Commit message') >= 0)
    return;

  sendResponse({file: file});
}

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (clicked_element && request == 'getFiles')
    handleGerritFilePaths(request, sendResponse);
  else if (clicked_element && request == 'getFile')
    handleGerritFile(request, sendResponse);
  clicked_element = null;
});
