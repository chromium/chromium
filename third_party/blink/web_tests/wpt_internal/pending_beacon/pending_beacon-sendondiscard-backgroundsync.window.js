// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/common/utils.js
// META: script=/pending_beacon/resources/pending_beacon-helper.js

'use strict';

promise_test(async t => {
  const uuid = token();

  // Create an iframe that contains a document that creates two beacons.
  const iframe = document.createElement('iframe');
  iframe.src =
      `/pending_beacon/resources/create_beacons_in_background_sync_denied_page.html?uuid=${
          uuid}`;

  const iframe_load_promise =
      new Promise(resolve => iframe.onload = () => resolve());
  document.body.appendChild(iframe);
  await iframe_load_promise;

  // Delete the iframe to trigger beacon sending.
  document.body.removeChild(iframe);

  // The iframe should not send any beacons.
  await expectBeacon(uuid, {count: 0});
}, 'BackgroundSync Disabled: Verify that a discarded document does not send its beacons.');

promise_test(async t => {
  const uuid = token();

  // Create an iframe that contains a document that creates two beacons.
  const iframe = document.createElement('iframe');
  iframe.src =
      `/pending_beacon/resources/create_beacons_in_background_sync_granted_page.html?uuid=${
          uuid}`;

  const iframe_load_promise =
      new Promise(resolve => iframe.onload = () => resolve());
  document.body.appendChild(iframe);
  await iframe_load_promise;

  // Delete the iframe to trigger beacon sending.
  document.body.removeChild(iframe);

  // The iframe should have sent 2 beacons.
  await expectBeacon(uuid, {count: 2});
}, 'BackgroundSync Granted: Verify that a discarded document sends its beacons.');
