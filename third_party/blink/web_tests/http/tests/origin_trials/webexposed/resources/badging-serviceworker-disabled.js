'use strict';

importScripts('/resources/testharness.js');

test(t => {
  assert_false(
      'setAppBadge' in navigator, 'setAppBadge does not exist in navigator');
  assert_false(
      'clearAppBadge' in navigator,
      'clearAppBadge does not exist in  navigator');
}, 'Badge API interfaces and properties in Origin-Trial disabled service worker.');

done();
