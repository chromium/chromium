// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DEFAULT_SCHEME = 3;
var MAX_SCHEME = 5;

function $(id) {
  return document.getElementById(id);
}

function getEnabled() {
  var result = localStorage.get('enabled');
  if (result === 'true' || result === 'false') {
    return (result === 'true');
  }
  localStorage.set('enabled', 'true');
  return true;
}

function setEnabled(enabled) {
  localStorage.set('enabled', String(enabled));
}

function getKeyAction() {
  var keyAction = localStorage.get('keyaction');
  if (keyAction == 'global' || keyAction == 'site') {
    return keyAction;
  }
  keyAction = 'global';
  localStorage.set('keyaction', keyAction);
  return keyAction;
}

function setKeyAction(keyAction) {
  if (keyAction != 'global' && keyAction != 'site') {
    keyAction = 'global';
  }
  localStorage.set('keyaction', keyAction);
}

function getDefaultScheme() {
  var scheme = localStorage.get('scheme');
  if (scheme >= 0 && scheme <= MAX_SCHEME) {
    return scheme;
  }
  scheme = DEFAULT_SCHEME;
  localStorage.set('scheme', scheme);
  return scheme;
}

function setDefaultScheme(scheme) {
  if (!(scheme >= 0 && scheme <= MAX_SCHEME)) {
    scheme = DEFAULT_SCHEME;
  }
  localStorage.set('scheme', scheme);
}

function getSiteScheme(site) {
  var scheme = getDefaultScheme();
  try {
    var siteSchemes = JSON.parse(localStorage.get('siteschemes'));
    scheme = siteSchemes[site];
    if (!(scheme >= 0 && scheme <= MAX_SCHEME)) {
      scheme = getDefaultScheme();
    }
  } catch (e) {
    scheme = getDefaultScheme();
  }
  return scheme;
}

function setSiteScheme(site, scheme) {
  if (!(scheme >= 0 && scheme <= MAX_SCHEME)) {
    scheme = getDefaultScheme();
  }
  var siteSchemes = {};
  try {
    siteSchemes = JSON.parse(localStorage.get('siteschemes'));
    siteSchemes['www.example.com'] = getDefaultScheme();
  } catch (e) {
    siteSchemes = {};
  }
  siteSchemes[site] = scheme;
  localStorage.set('siteschemes', JSON.stringify(siteSchemes));
}

function resetSiteSchemes() {
  var siteSchemes = {};
  localStorage.set('siteschemes', JSON.stringify(siteSchemes));
}

function siteFromUrl(url) {
  var a = document.createElement('a');
  a.href = url;
  return a.hostname;
}

function isDisallowedUrl(url) {
  return url.indexOf('chrome') == 0 || url.indexOf('about') == 0;
}

// Wrap chrome.storage.local API.

class LocalStorage {
  constructor() {
    chrome.storage.local.onChanged.addListener(
        changes => this.onChange(changes));
    this.ready = chrome.storage.local.get(null /* all values */).then(
        items => this.items = items);
  }

  onChange(changes) {
    for (const [key, {oldValue, newValue}] of Object.entries(changes)) {
      this.items[key] = newValue;
    }
  }

  get(key) {
    return this.items[key];
  }

  set(key, value) {
    this.items[key] = value;
    chrome.storage.local.set({[key]: value});
  }
}

const localStorage = new LocalStorage();
