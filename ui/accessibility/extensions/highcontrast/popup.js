// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var site;
var key1;
var key2;

function setRadio(name, value) {
  var radios = document.querySelectorAll('input[name="' + name + '"]');
  for (var i = 0; i < radios.length; i++) {
    radios[i].checked = (radios[i].value == value);
    radios[i].disabled = !getEnabled();
  }
}

function update() {
  document.body.className = getEnabled() ? '' : 'disabled';

  if (getEnabled()) {
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

  setRadio('keyaction', getKeyAction());
  if (site) {
    setRadio('scheme', getSiteScheme(site));
    $('make_default').disabled = (getSiteScheme(site) == getDefaultScheme());
  } else {
    setRadio('scheme', getDefaultScheme());
  }
  if (getEnabled()) {
    document.documentElement.setAttribute(
        'hc',
        site ? 'a' + getSiteScheme(site) : 'a' + getDefaultScheme());
  } else {
    document.documentElement.setAttribute('hc', 'a0');
  }
}

function onToggle() {
  setEnabled(!getEnabled());
  update();
}

function onForget() {
  resetSiteSchemes();
  update();
}

function onRadioChange(name, value) {
  switch (name) {
    case 'keyaction':
      setKeyAction(value);
      break;
    case 'apply':
      setApply(value);
      break;
    case 'scheme':
      if (site) {
        setSiteScheme(site, value);
      } else {
        setDefaultScheme(value);
      }
      break;
  }
  update();
}

function onMakeDefault() {
  setDefaultScheme(getSiteScheme(site));
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
