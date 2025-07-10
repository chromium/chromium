// Copyright (c) 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDefaultScheme, getEnabled, getSiteScheme, isMacOS, MIN_SCHEME, setEnabled, setSiteScheme, siteFromUrl,} from './common.js';

const injectContentScripts = async(): Promise<void> => {
  const windows = await chrome.windows.getAll({populate: true});

  for (const w of windows) {
    for (const tab of w.tabs ?? []) {
      const url = tab.url;
      if (!url || url.startsWith('chrome') || url.startsWith('about')) {
        continue;
      }

      await chrome.scripting.executeScript({
        target: {tabId: tab.id!, allFrames: true},
        files: ['highcontrast.js'],
        injectImmediately: true,
      });
    }
  }
};

const toggleEnabled = async(): Promise<void> => {
  const current = await getEnabled();
  await setEnabled(!current);
};

const toggleSite = async(url: string): Promise<void> => {
  const site = siteFromUrl(url);
  const currentScheme = await getSiteScheme(site);
  const scheme =
      currentScheme > MIN_SCHEME ? MIN_SCHEME : await getDefaultScheme();

  await setSiteScheme(site, scheme);
};

chrome.runtime.onInstalled.addListener((): void => {
  void injectContentScripts();
});

chrome.runtime.onMessage.addListener(
    (message: any, sender: chrome.runtime.MessageSender,
     _: (response?: any) => void): boolean => {
      if (message['toggle_global']) {
        toggleEnabled();
      }
      if (message['toggle_site']) {
        const url = sender.tab?.url ?? 'https://www.example.com';
        toggleSite(url);
      }

      return false;
    });

if (isMacOS()) {
  chrome.action.setTitle({
    title: 'High Contrast (Cmd+Shift+F11)',
  });
}
