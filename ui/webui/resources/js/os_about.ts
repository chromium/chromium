// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from './util_ts.js';

getRequiredElement('os-link-href').onclick = crosUrlAboutRedirect;

function crosUrlAboutRedirect() {
  chrome.send('crosUrlAboutRedirect');
}
