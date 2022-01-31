// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var site;
var key1;
var key2;
Storage.initialize();

function setRadio(name, value) {
  var radios = document.querySelectorAll('input[name="' + name + '"]');
  for (var i = 0; i < radios.length; i++) {
    radios[i].checked = (radios[i].value == value);
    radios[i].disabled = !Storage.enabled;
  }
}

function update() {
  document.body.className = Storage.enabled ? '' : 'disabled';

  if (Storage.enabled) {
    $('title').innerText = chrome.i18n.getMessage('highcontrast_enabled');
    $('toggle').innerHTML =
        '<b>' + chrome.i18n.getMessage('highcontrast_disable') + '</b><br>' +
        '<span class="kb">(' + key1 + ')</span>';
    $('subcontrols').style.display = 'block';
  } else {
    $('title').innerText = chrome.i18n.getMessage('highcontrast_disabled');
    $('toggle').innerHTML =
        '<b>' + chrome.i18n.getMessage('highcontrast_enable') + '</b><br>' +
        '<span class="kb">(' + key1 + ')</span>';
    $('subcontrols').style.display = 'none';
  }

  setRadio('keyaction', Storage.keyAction);
  if (site) {
    const scheme = Storage.getSiteScheme(site);
    setRadio('scheme', scheme);
    $('make_default').disabled = (scheme == Storage.scheme);
  } else {
    setRadio('scheme', Storage.scheme);
  }
  if (Storage.enabled) {
    document.documentElement.setAttribute(
        'hc',
        site ? 'a' + Storage.getSiteScheme(site) : 'a' + Storage.scheme);
  } else {
    document.documentElement.setAttribute('hc', 'a0');
  }
  chrome.extension.getBackgroundPage().updateTabs();
}

function onToggle() {
  Storage.enabled = !Storage.enabled;
  update();
}

function onForget() {
  Storage.resetSiteSchemes();
  update();
}

function onRadioChange(name, value) {
  switch (name) {
    case 'keyaction':
      Storage.keyAction = value;
      break;
    case 'apply':
      Storage.enabled = value;
      break;
    case 'scheme':
      if (site) {
        Storage.setSiteScheme(site, value);
      } else {
        Storage.scheme = value;
      }
      break;
  }
  update();
}

function onMakeDefault() {
  Storage.scheme = Storage.getSiteScheme(site);
  update();
}

function addRadioListeners(name) {
  var radios = document.querySelectorAll('input[name="' + name + '"]');
  for (var i = 0; i < radios.length; i++) {
    radios[i].addEventListener('change', function(evt) {
      onRadioChange(evt.target.name, evt.target.value);
    }, false);
    radios[i].addEventListener('click', function(evt) {
      onRadioChange(evt.target.name, evt.target.value);
    }, false);
  }
}

function init() {
  var i18nElements = document.querySelectorAll('*[i18n-content]');
  for (var i = 0; i < i18nElements.length; i++) {
    var elem = i18nElements[i];
    var msg = elem.getAttribute('i18n-content');
    elem.innerHTML = chrome.i18n.getMessage(msg);
  }

  addRadioListeners('keyaction');
  addRadioListeners('apply');
  addRadioListeners('scheme');
  $('toggle').addEventListener('click', onToggle, false);
  $('make_default').addEventListener('click', onMakeDefault, false);
  $('forget').addEventListener('click', onForget, false);
  if (navigator.appVersion.indexOf('Mac') != -1) {
    key1 = '&#x2318;+Shift+F11';
    key2 = '&#x2318;+Shift+F12';
  } else {
    key1 = 'Shift+F11';
    key2 = 'Shift+F12';
  }

  chrome.windows.getLastFocused({'populate': true}, function(window) {
    for (var i = 0; i < window.tabs.length; i++) {
      var tab = window.tabs[i];
      if (tab.active) {
        if (isDisallowedUrl(tab.url)) {
          $('scheme_title').innerText =
              chrome.i18n.getMessage('highcontrast_default');
          $('make_default').style.display = 'none';
        } else {
          site = siteFromUrl(tab.url);
          $('scheme_title').innerHTML =
              chrome.i18n.getMessage('highcontrast_',
                  '<b>' + site + '</b>:<br>' +
                  '<span class="kb">(' + key2 + ')</span>');
          $('make_default').style.display = 'block';
        }
        update();
        return;
      }
    }
    site = 'unknown site';
    update();
  });
}

window.addEventListener('load', init, false);
