// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DEFAULT_SCHEME = 3;
var MAX_SCHEME = 5;

function $(id) {
  return document.getElementById(id);
}

function getEnabled() {
  var result = localStorage['enabled'];
  if (result === 'true' || result === 'false') {
    return (result === 'true');
  }
  localStorage['enabled'] = 'true';
  return true;
}

function setEnabled(enabled) {
  localStorage['enabled'] = enabled;
}

function getKeyAction() {
  var keyAction = localStorage['keyaction'];
  if (keyAction == 'global' || keyAction == 'site') {
    return keyAction;
  }
  keyAction = 'global';
  localStorage['keyaction'] = keyAction;
  return keyAction;
}

function setKeyAction(keyAction) {
  if (keyAction != 'global' && keyAction != 'site') {
    keyAction = 'global';
  }
  localStorage['keyaction'] = keyAction;
}

function getDefaultScheme() {
  var scheme = localStorage['scheme'];
  if (scheme >= 0 && scheme <= MAX_SCHEME) {
    return scheme;
  }
  scheme = DEFAULT_SCHEME;
  localStorage['scheme'] = scheme;
  return scheme;
}

function setDefaultScheme(scheme) {
  if (!(scheme >= 0 && scheme <= MAX_SCHEME)) {
    scheme = DEFAULT_SCHEME;
  }
  localStorage['scheme'] = scheme;
}

function getSiteScheme(site) {
  var scheme = getDefaultScheme();
  try {
    var siteSchemes = JSON.parse(localStorage['siteschemes']);
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
    siteSchemes = JSON.parse(localStorage['siteschemes']);
    siteSchemes['www.example.com'] = getDefaultScheme();
  } catch (e) {
    siteSchemes = {};
  }
  siteSchemes[site] = scheme;
  localStorage['siteschemes'] = JSON.stringify(siteSchemes);
}

function resetSiteSchemes() {
  var siteSchemes = {};
  localStorage['siteschemes'] = JSON.stringify(siteSchemes);
}

function siteFromUrl(url) {
  var a = document.createElement('a');
  a.href = url;
  return a.hostname;
}

function isDisallowedUrl(url) {
  return url.startsWith('chrome') || url.startsWith('about');
}
