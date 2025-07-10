// Copyright (c) 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license.

import {$, getDefaultScheme, getEnabled, getKeyAction, getSiteScheme, isDisallowedUrl, isMacOS, resetSiteSchemes, setDefaultScheme, setEnabled, setKeyAction, setSiteScheme, siteFromUrl,} from './common.js';

let site: string|null = null;
let key1: string;
let key2: string;

const setRadio = (name: string, value: string|number, disabled?: boolean) => {
  const radios =
      document.querySelectorAll<HTMLInputElement>(`input[name="${name}"]`);
  radios.forEach((radio) => {
    radio.checked = radio.value == String(value);
    if (disabled !== undefined) {
      radio.disabled = disabled;
    }
  });
};

const update = async(): Promise<void> => {
  const enabled = await getEnabled();
  document.body.className = enabled ? '' : 'disabled';

  const title = $('title');
  const toggle = $('toggle');
  const subcontrols = $('subcontrols') as HTMLElement | null;
  const makeDefault = $('make_default') as HTMLButtonElement | null;

  if (enabled) {
    title!.innerText = chrome.i18n.getMessage('highcontrast_enabled');
    toggle!.innerHTML = `<b>${
        chrome.i18n.getMessage(
            'highcontrast_disable')}</b><br><span class="kb">(${key1})</span>`;
    subcontrols!.style.display = 'block';
  } else {
    title!.innerText = chrome.i18n.getMessage('highcontrast_disabled');
    toggle!.innerHTML = `<b>${
        chrome.i18n.getMessage(
            'highcontrast_enable')}</b><br><span class="kb">(${key1})</span>`;
    subcontrols!.style.display = 'none';
  }

  const keyActionValue = await getKeyAction();
  setRadio('keyaction', keyActionValue, !enabled);

  if (site) {
    const [siteScheme, defaultScheme] = await Promise.all([
      getSiteScheme(site),
      getDefaultScheme(),
    ]);
    setRadio('scheme', siteScheme, !enabled);
    if (makeDefault)
      makeDefault.disabled = siteScheme === defaultScheme;
  } else {
    const defaultScheme = await getDefaultScheme();
    setRadio('scheme', defaultScheme, !enabled);
  }

  const scheme = site ? await getSiteScheme(site) : await getDefaultScheme();
  document.documentElement.setAttribute('hc', enabled ? 'a' + scheme : 'a0');
};

const onToggle = async(): Promise<void> => {
  const current = await getEnabled();
  await setEnabled(!current);
  await update();
};

const onForget = async(): Promise<void> => {
  await resetSiteSchemes();
  await update();
};

const onRadioChange = async(name: string, value: string): Promise<void> => {
  switch (name) {
    case 'keyaction':
      await setKeyAction(value);
      break;
    case 'scheme':
      const numeric = parseInt(value, 10);
      if (site) {
        await setSiteScheme(site, numeric);
      } else {
        await setDefaultScheme(numeric);
      }
      break;
  }
  await update();
};

const onMakeDefault = async(): Promise<void> => {
  if (!site)
    return;
  const siteScheme = await getSiteScheme(site);
  await setDefaultScheme(siteScheme);
  await update();
};

const addRadioListeners = (name: string): void => {
  const radios =
      document.querySelectorAll<HTMLInputElement>(`input[name="${name}"]`);
  radios.forEach((radio) => {
    const handler = (evt: Event) => {
      const target = evt.target as HTMLInputElement;
      onRadioChange(target.name, target.value);
    };
    radio.addEventListener('change', handler, false);
    radio.addEventListener('click', handler, false);
  });
};

const setKeyboardShortcuts = (): void => {
  if (isMacOS()) {
    key1 = '&#x2318;+Shift+F11';
    key2 = '&#x2318;+Shift+F12';
  } else {
    key1 = 'Shift+F11';
    key2 = 'Shift+F12';
  }
};

const init = async(): Promise<void> => {
  const i18nElements = document.querySelectorAll<HTMLElement>('[i18n-content]');
  i18nElements.forEach((elem) => {
    const msg = elem.getAttribute('i18n-content');
    if (msg) {
      elem.innerHTML = chrome.i18n.getMessage(msg);
    }
  });

  addRadioListeners('keyaction');
  addRadioListeners('scheme');

  $('toggle')?.addEventListener('click', onToggle, false);
  $('make_default')?.addEventListener('click', onMakeDefault, false);
  $('forget')?.addEventListener('click', onForget, false);

  setKeyboardShortcuts();

  const win = await chrome.windows.getLastFocused({populate: true});
  const activeTab = win.tabs?.find((tab) => tab.active);

  const schemeTitle = $('scheme_title');
  const makeDefault = $('make_default');

  if (activeTab?.url && !isDisallowedUrl(activeTab.url)) {
    site = siteFromUrl(activeTab.url);
    schemeTitle!.innerHTML = chrome.i18n.getMessage(
        'highcontrast_',
        `<b>${site}</b>:<br><span class="kb">(${key2})</span>`);
    makeDefault!.style.display = 'block';
  } else {
    schemeTitle!.innerText = chrome.i18n.getMessage('highcontrast_default');
    makeDefault!.style.display = 'none';
  }

  await update();
};

window.addEventListener('load', init, false);
