// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Popup {
  constructor() {
    /** @type {string} */
    this.site;

    /** @type {string} */
    this.key1;

    /** @type {string} */
    this.key2;

    this.init();
  }

  /** @param {number} value */
  setSchemeRadio(value) {
    const radios = document.querySelectorAll('input[name="scheme"]');
    for (let i = 0; i < radios.length; i++) {
      radios[i].checked = (radios[i].value == value);
      radios[i].disabled = !Storage.enabled;
    }
  }

  update() {
    document.body.className = Storage.enabled ? '' : 'disabled';

    if (Storage.enabled) {
      $('title').innerText = chrome.i18n.getMessage('highcontrast_enabled');
      $('toggle').innerHTML = '<b>' +
          chrome.i18n.getMessage('highcontrast_disable') + '</b><br>' +
          '<span class="kb">(' + this.key1 + ')</span>';
      $('subcontrols').style.display = 'block';
    } else {
      $('title').innerText = chrome.i18n.getMessage('highcontrast_disabled');
      $('toggle').innerHTML = '<b>' +
          chrome.i18n.getMessage('highcontrast_enable') + '</b><br>' +
          '<span class="kb">(' + this.key1 + ')</span>';
      $('subcontrols').style.display = 'none';
    }

    if (this.site) {
      const scheme = Storage.getSiteScheme(this.site);
      this.setSchemeRadio(scheme);
      $('make_default').disabled = (scheme == Storage.scheme);
    } else {
      this.setSchemeRadio(Storage.scheme);
    }
    if (Storage.enabled) {
      document.documentElement.setAttribute(
          'hc',
          this.site ? 'a' + Storage.getSiteScheme(this.site) :
                      'a' + Storage.scheme);
    } else {
      document.documentElement.setAttribute('hc', 'a0');
    }
    chrome.extension.sendRequest({updateTabs: true});
  }

  /** @param {number} value */
  onSchemeChange(value) {
    if (this.site) {
      Storage.setSiteScheme(this.site, value);
    } else {
      Storage.scheme = value;
    }
  }

  onToggle() {
    Storage.enabled = !Storage.enabled;
    this.update();
  }

  onForget() {
    Storage.resetSiteSchemes();
    this.update();
  }

  onMakeDefault() {
    Storage.scheme = Storage.getSiteScheme(this.site);
    this.update();
  }

  addRadioListeners() {
    const radios = document.querySelectorAll('input[name="scheme"]');
    for (let i = 0; i < radios.length; i++) {
      radios[i].addEventListener('change', (event) => {
        this.onSchemeChange(Number(event.target.value));
      }, false);
      radios[i].addEventListener('click', (event) => {
        this.onSchemeChange(Number(event.target.value));
      }, false);
    }
  }

  init() {
    const i18nElements = document.querySelectorAll('*[i18n-content]');
    for (let i = 0; i < i18nElements.length; i++) {
      const elem = i18nElements[i];
      const msg = elem.getAttribute('i18n-content');
      elem.innerHTML = chrome.i18n.getMessage(msg);
    }

    this.addRadioListeners();
    $('toggle').addEventListener('click', this.onToggle.bind(this), false);
    $('make_default')
        .addEventListener('click', this.onMakeDefault.bind(this), false);
    $('forget').addEventListener('click', this.onForget.bind(this), false);
    if (navigator.appVersion.indexOf('Mac') != -1) {
      this.key1 = '&#x2318;+Shift+F11';
      this.key2 = '&#x2318;+Shift+F12';
    } else {
      this.key1 = 'Shift+F11';
      this.key2 = 'Shift+F12';
    }

    chrome.windows.getLastFocused({'populate': true}, (window) => {
      for (let i = 0; i < window.tabs.length; i++) {
        const tab = window.tabs[i];
        if (tab.active) {
          if (isDisallowedUrl(tab.url)) {
            $('scheme_title').innerText =
                chrome.i18n.getMessage('highcontrast_default');
            $('make_default').style.display = 'none';
          } else {
            this.site = siteFromUrl(tab.url);
            $('scheme_title').innerHTML = chrome.i18n.getMessage(
                'highcontrast_',
                '<b>' + this.site + '</b>:<br>' +
                    '<span class="kb">(' + this.key2 + ')</span>');
            $('make_default').style.display = 'block';
          }
          this.update();
          return;
        }
      }
      this.site = 'unknown site';
      this.update();
    });
  }
}

window.addEventListener('load', () => new Popup(), false);
Storage.initialize();
