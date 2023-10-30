// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from './util.js';

getRequiredElement('os-link-href').onclick = crosUrlAboutRedirect;

// trigger the click handler for middle-button clicks
getRequiredElement('os-link-href').onauxclick = ((event: MouseEvent) => {
                                                  if (event.button === 1) {
                                                    crosUrlAboutRedirect(event);
                                                  }
                                                }) as EventListener;

function crosUrlAboutRedirect(event: Event) {
  event.preventDefault();
  chrome.send('crosUrlAboutRedirect');
}
