// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/pending-beacon/resources/pending_beacon-helper.js
// META: timeout=long

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
  // Enables BackgroundSync permission such that deferred request won't be
  // immediately sent out on entering BFCache.
  await setBackgroundSyncEnabled(true);

  const uuid = token();
  const url = generateSetBeaconURL(uuid);
  // Sets no option to test the default behavior when a document enters BFCache.
  const helper = new RemoteContextHelper();
  // Opens a window with noopener so that BFCache will work.
  const rc1 = await helper.addWindow(
      /*config=*/ null, /*options=*/ {features: 'noopener'});

  // Creates a fetchLater request with default config in remote, which should
  // only be sent on page discarded (not on entering BFCache).
  await rc1.executeScript(url => {
    fetchLater(url);
    // Add a pageshow listener to stash the BFCache event.
    window.addEventListener('pageshow', e => {
      window.pageshowEvent = e;
    });
  }, [url]);
  // Navigates away to let page enter BFCache.
  const rc2 = await rc1.navigateToNew();
  // Navigates back.
  await rc2.historyBack();
  // Verifies the page was BFCached.
  assert_true(await rc1.executeScript(() => {
    return window.pageshowEvent.persisted;
  }));

  // By default, pending requests are all flushed on BFCache no matter
  // BackgroundSync is on or not. See http://b/310541607#comment28.
  await expectBeacon(uuid, {count: 1});
}, `fetchLater() does send on page entering BFCache even if BackgroundSync is on.`);

parallelPromiseTest(async t => {
  // Enables BackgroundSync permission such that deferred request won't be
  // immediately sent out on entering BFCache.
  await setBackgroundSyncEnabled(true);

  const uuid = token();
  const url = generateSetBeaconURL(uuid);
  // activateAfter = 0s means the request should be sent out right on
  // document becoming deactivated (BFCached or frozen) after navigating away.
  const options = {activateAfter: 0};
  const helper = new RemoteContextHelper();
  // Opens a window with noopener so that BFCache will work.
  const rc1 = await helper.addWindow(
      /*config=*/ null, /*options=*/ {features: 'noopener'});

  // Creates a fetchLater request in remote which should only be sent on
  // navigating away.
  await rc1.executeScript((url, options) => {
    fetchLater(url, options);

    // Add a pageshow listener to stash the BFCache event.
    window.addEventListener('pageshow', e => {
      window.pageshowEvent = e;
    });
  }, [url, options]);
  // Navigates away to trigger request sending.
  const rc2 = await rc1.navigateToNew();
  // Navigates back.
  await rc2.historyBack();
  // Verifies the page was BFCached.
  assert_true(await rc1.executeScript(() => {
    return window.pageshowEvent.persisted;
  }));

  await expectBeacon(uuid, {count: 1});
}, `fetchLater() with activateAfter=0 sends on page entering BFCache if BackgroundSync is on.`);

parallelPromiseTest(async t => {
  // Enables BackgroundSync permission such that deferred request won't be
  // immediately sent out on entering BFCache.
  await setBackgroundSyncEnabled(true);

  const uuid = token();
  const url = generateSetBeaconURL(uuid);
  // activateAfter = 1m means the request should NOT be sent out on
  // document becoming deactivated (BFCached or frozen) until after 1 minute.
  const options = {activateAfter: 60000};
  const helper = new RemoteContextHelper();
  // Opens a window with noopener so that BFCache will work.
  const rc1 = await helper.addWindow(
      /*config=*/ null, /*options=*/ {features: 'noopener'});

  // Creates a fetchLater request in remote which should only be sent on
  // navigating away.
  await rc1.executeScript((url, options) => {
    fetchLater(url, options);

    // Adds a pageshow listener to stash the BFCache event.
    window.addEventListener('pageshow', e => {
      window.pageshowEvent = e;
    });
  }, [url, options]);
  // Navigates away to trigger request sending.
  const rc2 = await rc1.navigateToNew();
  // Navigates back.
  await rc2.historyBack();
  // Verifies the page was BFCached.
  assert_true(await rc1.executeScript(() => {
    return window.pageshowEvent.persisted;
  }));

  // By default, pending requests are all flushed on BFCache no matter
  // BackgroundSync is on or not. See http://b/310541607#comment28.
  await expectBeacon(uuid, {count: 1});
}, `fetchLater() with activateAfter=1m does send on page entering BFCache even if BackgroundSync is on.`);
