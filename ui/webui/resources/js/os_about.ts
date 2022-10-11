// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

$('os-link-href').onclick = crosUrlAboutRedirect;

function crosUrlAboutRedirect() {
  chrome.send('crosUrlAboutRedirect');
}
