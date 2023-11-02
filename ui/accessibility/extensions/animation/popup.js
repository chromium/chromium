// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function animationPolicyChanged() {
  if (this.checked) {
    const type = this.id;
    const setting = this.value;
    console.log('set policy '+': '+setting);
    chrome.accessibilityFeatures.animationPolicy.set(
        {'value': setting}, function (callback) {});
  }
}

function listener(data) {
  console.log('animation policy is changed.');
}

function init() {
  const i18nElements = document.querySelectorAll('*[i18n-content]');
  for (let i = 0; i < i18nElements.length; i++) {
    const elem = i18nElements[i];
    const msg = elem.getAttribute('i18n-content');
    elem.innerHTML = chrome.i18n.getMessage(msg);
  }

  chrome.accessibilityFeatures.animationPolicy.onChange.addListener(listener);
  chrome.accessibilityFeatures.animationPolicy.get(
        {'incognito': false}, function (policy) {
                  console.log('get policy '+': '+policy.value);
      const selects = document.querySelectorAll('input');
      for (let i = 0; i < selects.length; i++) {
        if (selects[i].value == policy.value)
          selects[i].checked = true;
      }
    });

  const selects = document.querySelectorAll('input');
  for (let i = 0; i < selects.length; i++) {
    selects[i].addEventListener('change', animationPolicyChanged);
  }
}

window.addEventListener('load', init, false);
