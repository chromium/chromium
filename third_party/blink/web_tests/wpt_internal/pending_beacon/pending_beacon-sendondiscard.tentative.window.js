// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/common/utils.js
// META: script=/pending_beacon/resources/pending_beacon-helper.js

'use strict';

/**
 * This file cannot be upstreamed to external/wpt/ until:
 * `internals.setPermission()` usage is replaced with a WebDriver API.
 */

// BackgroundSync needs to be explicitly enabled in Web Tests, as the test
// runner uses a different permission manager. See
// https://source.chromium.org/chromium/chromium/src/+/main:content/web_test/browser/web_test_permission_manager.h;l=138-140;drc=f616c54d73c8eea9db5f7e567611711897651b66
async function setBackgroundSyncEnabled(enabled) {
  const status = enabled ? 'granted' : 'denied';
  const origin = location.origin;
  await internals.setPermission(
      {name: 'background-sync'}, status, origin, origin);
}

parallelPromiseTest(async t => {
  const uuid = token();
  const url = `/pending_beacon/resources/set_beacon.py?uuid=${uuid}`;
  const numPerMethod = 20;
  const total = numPerMethod * 2;
  // "Sending beacon on page discard" requires BackgroundSync permission.
  await setBackgroundSyncEnabled(true);

  // Loads an iframe that creates `numPerMethod` GET & POST beacons.
  const iframe = await loadScriptAsIframe(`
    const url = "${url}";
    for (let i = 0; i < ${numPerMethod}; i++) {
      let get = new PendingGetBeacon(url);
      let post = new PendingPostBeacon(url);
    }
  `);

  // Delete the iframe to trigger beacon sending.
  document.body.removeChild(iframe);

  // The iframe should have sent all beacons.
  await expectBeacon(uuid, {count: total});
}, 'A discarded document sends all its beacons with default config.');

parallelPromiseTest(async t => {
  const uuid = token();
  const url = `/pending_beacon/resources/set_beacon.py?uuid=${uuid}`;
  // "Sending beacon on page discard" requires BackgroundSync permission.
  await setBackgroundSyncEnabled(true);

  // Loads an iframe that creates a GET beacon,
  // then sends it out with `sendNow()`.
  const iframe = await loadScriptAsIframe(`
    const url = "${url}";
    let beacon = new PendingGetBeacon(url);
    beacon.sendNow();
  `);

  // Delete the document and verify no more beacons are sent.
  document.body.removeChild(iframe);

  // The iframe should have sent only 1 beacon.
  await expectBeacon(uuid, {count: 1});
}, 'A discarded document does not send an already sent beacon.');

parallelPromiseTest(async t => {
  const uuid = token();
  const url = `/pending_beacon/resources/set_beacon.py?uuid=${uuid}`;
  const numPerMethod = 20;
  const total = numPerMethod * 2;
  // "Sending beacon on page discard" requires BackgroundSync permission.
  await setBackgroundSyncEnabled(true);

  // Loads an iframe that creates `numPerMethod` GET & POST beacons with
  // different timeouts.
  const iframe = await loadScriptAsIframe(`
    const url = "${url}";
    for (let i = 0; i < ${numPerMethod}; i++) {
      let get = new PendingGetBeacon(url, {timeout: 100*i});
      let post = new PendingPostBeacon(url, {timeout: 100*i});
    }
  `);

  // Delete the iframe to trigger beacon sending.
  document.body.removeChild(iframe);

  // Even beacons are configured with different timeouts,
  // the iframe should have sent all beacons when it is discarded.
  await expectBeacon(uuid, {count: total});
}, `A discarded document sends all its beacons of which timeouts are not
    default.`);

parallelPromiseTest(async t => {
  const uuid = token();
  const url = `/pending_beacon/resources/set_beacon.py?uuid=${uuid}`;
  const numPerMethod = 20;
  const total = numPerMethod * 2;
  // "Sending beacon on page discard" requires BackgroundSync permission.
  await setBackgroundSyncEnabled(true);

  // Loads an iframe that creates `numPerMethod` GET & POST beacons with
  // different backgroundTimeouts.
  const iframe = await loadScriptAsIframe(`
    const url = "${url}";
    for (let i = 0; i < ${numPerMethod}; i++) {
      let get = new PendingGetBeacon(url, {backgroundTimeout: 100*i});
      let post = new PendingPostBeacon(url, {backgroundTimeout: 100*i});
    }
  `);

  // Delete the iframe to trigger beacon sending.
  document.body.removeChild(iframe);

  // Even beacons are configured with different backgroundTimeouts,
  // the iframe should have sent all beacons when it is discarded.
  await expectBeacon(uuid, {count: total});
}, `A discarded document sends all its beacons of which backgroundTimeouts are
    not default.`);
