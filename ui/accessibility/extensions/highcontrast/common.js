// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(id) {
  return document.getElementById(id);
}

function siteFromUrl(url) {
  return new URL(url).hostname;
}

function isDisallowedUrl(url) {
  return url.startsWith('chrome') || url.startsWith('about');
}
