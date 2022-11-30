/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

Storage.initialize();

function setRadio(name, defaultValue) {
  let value = Storage[name];
  const controls = document.querySelectorAll(
      'input[type="radio"][name="' + name + '"]');
  for (let i = 0; i < controls.length; i++) {
    const c = controls[i];
    if (c.value == value) {
      c.checked = true;
    }
    c.addEventListener('change', function(evt) {
      if (evt.target.checked) {
        Storage[evt.target.name] = evt.target.value;
      }
    }, false);
  }
}

function load() {
  const isMac = (navigator.appVersion.indexOf("Mac") != -1);
  if (isMac) {
    document.body.classList.add('mac');
  } else {
    document.body.classList.add('nonmac');
  }

  const isCros = (navigator.appVersion.indexOf("CrOS") != -1);
  if (isCros) {
    document.body.classList.add('cros');
  } else {
    document.body.classList.add('noncros');
  }

  setRadio('onenable', 'anim');
  setRadio('onjump', 'flash');

  const heading = document.querySelector('h1');
  const sel = window.getSelection();
  sel.setBaseAndExtent(heading, 0, heading, 0);

  document.title =
      chrome.i18n.getMessage('caret_browsing_caretBrowsingOptions');
  const i18nElements = document.querySelectorAll('*[i18n-content]');
  for (let i = 0; i < i18nElements.length; i++) {
    const elem = i18nElements[i];
    const msg = elem.getAttribute('i18n-content');
    elem.innerHTML = chrome.i18n.getMessage(msg);
  }
}

window.addEventListener('load', load, false);
