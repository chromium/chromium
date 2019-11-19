// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Returns provider name of the given testing provider manifest viz., the
   * the value of the name field in the |manifest| file.
   * @param {string} manifest Testing provider manifest file name.
   * @return {string} Testing provider name.
   */
  function getProviderNameForTest(manifest) {
    if (manifest === 'manifest.json') {
      return 'Files Testing Provider test extension';
    }
    if (manifest === 'manifest_multiple_mounts.json') {
      return 'Files Testing Provider multiple mounts test extension';
    }
    if (manifest === 'manifest_source_device.json') {
      return 'Files Testing Provider device test extension';
    }
    if (manifest === 'manifest_source_file.json') {
      return 'Files Testing Provider file test extension';
    }

    throw new Error('unknown mainfest: '.concat(manifest));
  }

  /**
   * Initializes the provider extension.
   * @param {string} manifest The manifest name of testing provider extension
   *     to launch for the test case.
   */
  async function setUpProvider(manifest) {
    await sendTestMessage(
        {name: 'launchProviderExtension', manifest: manifest});
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
    return appId;
  }

  /**
   * Clicks on the gear menu.
   */
  async function clickGearMenu(appId) {
    const newServiceMenuItem = '#gear-menu-newservice:not([hidden])';

    // Open the gear menu by clicking the gear button.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId, ['#gear-button']),
        'fakeMouseClick failed');

    // Wait for Add new service menu item to appear in the gear menu.
    return remoteCall.waitForElement(appId, newServiceMenuItem);
  }

  /**
   * Clicks on the "Add new services" menu button.
   */
  async function showProvidersMenu(appId) {
    const newServiceMenuItem = '#gear-menu-newservice:not([hidden])';

    // Open the gear menu by clicking the gear button.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId, ['#gear-button']),
        'fakeMouseClick failed');

    // Wait for Add new service menu item to appear.
    await remoteCall.waitForElement(appId, newServiceMenuItem);

    // Click the menu item.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId, [newServiceMenuItem]),
        'fakeMouseClick failed');
  }

  /**
   * Confirms that a provided volume is mounted.
   */
  async function confirmVolume(appId, ejectExpected) {
    await remoteCall.waitForElement(
        appId, '.tree-row .icon[volume-type-icon="provided"]');
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId,
            ['.tree-row .icon[volume-type-icon="provided"]']),
        'fakeMouseClick failed');


    await remoteCall.waitForElement(
        appId, '.tree-row[selected] .icon[volume-type-icon="provided"]');
    if (ejectExpected) {
      await remoteCall.waitForElement(appId, '.tree-row[selected] .root-eject');
    } else {
      await remoteCall.waitForElementLost(
          appId, '.tree-row[selected] .root-eject');
    }
  }

  /**
   * Tests that a provided extension with |manifest| is mountable via the menu
   * button.
   *
   * @param {boolean} multipleMounts Whether multiple mounts are supported by
   *     the providing extension.
   * @param {string} manifest Name of the manifest file for the providing
   *     extension.
   */
  async function requestMountInternal(multipleMounts, manifest) {
    const providerName = getProviderNameForTest(manifest);
    const appId = await setUpProvider(manifest);
    await showProvidersMenu(appId);

    // Wait for providers menu and new service menu item to appear.
    let result = await remoteCall.waitForElement(
        appId,
        '#add-new-services-menu:not([hidden]) cr-menu-item:first-child span');

    // Click to install test provider.
    chrome.test.assertEq(providerName, result.text);
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId,
            ['#add-new-services-menu cr-menu-item:first-child span']),
        'fakeMouseClick failed');

    await confirmVolume(appId, false /* ejectExpected */);

    // If multipleMounts we display the providers menu and check the provider is
    // still listed.
    if (multipleMounts) {
      await showProvidersMenu(appId);
      const selector =
          '#add-new-services-menu:not([hidden]) cr-menu-item:first-child ' +
          'span';
      result = await remoteCall.waitForElement(appId, selector);
      chrome.test.assertEq(providerName, result.text);
      return;
    }

    // If !multipleMounts and !isSmbEnabled, we open the gear menu and check the
    // "add new service" menu item. add-new-servuces goes directly to
    // install-new-extension, however install-new-service command uses webview
    // which doesn't work in the integration tests.
    const isSmbEnabled =
        await sendTestMessage({name: 'isSmbEnabled'}) === 'true';
    if (!isSmbEnabled) {
      await clickGearMenu(appId);
      const selector = '#gear-menu:not([hidden]) ' +
          'cr-menu-item[command="#install-new-extension"]';
      result = await remoteCall.waitForElement(appId, selector);
      return;
    }

    // If !multipleMounts but isSmbEnabled, we display the provider menu and
    // check the provider is not listed.
    await showProvidersMenu(appId);
    const selector =
        '#add-new-services-menu:not([hidden]) cr-menu-item:first-child ' +
        'span';
    result = await remoteCall.waitForElement(appId, selector);
    chrome.test.assertFalse(providerName === result.text);
  }

  /**
   * Tests that a provided extension with |manifest| is not available in the
   * button menu, but it's mounted automatically.
   *
   * @param {string} manifest Name of the manifest file for the providing
   *     extension.
   */
  async function requestMountNotInMenuInternal(manifest) {
    const appId = await setUpProvider(manifest);
    await confirmVolume(appId, true /* ejectExpected */);
    const element = await clickGearMenu(appId);

    const isSmbEnabled =
        await sendTestMessage({name: 'isSmbEnabled'}) === 'true';

    if (!isSmbEnabled) {
      // Here we only check these attributes because the menu item calls
      // Webstore using webview which doesn't work in the integration test.
      chrome.test.assertEq('Install new service', element.text);
      // Since there is no FSP provider, there should be no add-new-service
      // sub-menu, it should instead point to CWS install-new-extension.
      chrome.test.assertEq(
          '#install-new-extension', element.attributes.command);
      return;
    }

    // Since a provider is installed (here isSmbEnabled), we need to test that
    // 'add-new-service' sub-menu does not contain the |manifest| provider.
    chrome.test.assertTrue(isSmbEnabled);
    chrome.test.assertEq('Add new service', element.text);
    chrome.test.assertEq('#new-service', element.attributes.command);
    chrome.test.assertEq(
        '#add-new-services-menu', element.attributes['sub-menu']);

    // Extract 'add-new-service' sub-menu items.
    const selector = ['#add-new-services-menu[hidden] cr-menu-item'];
    const submenu = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, selector);

    // Check the sub-menu do not contain the |manifest| provider.
    chrome.test.assertEq(2, submenu.length);
    chrome.test.assertEq('SMB file share', submenu[0].text);
    chrome.test.assertEq('Install new service', submenu[1].text);
  }

  /**
   * Tests mounting a single mount point in the button menu.
   */
  testcase.requestMount = () => {
    const multipleMounts = false;
    return requestMountInternal(multipleMounts, 'manifest.json');
  };

  /**
   * Tests mounting multiple mount points in the button menu.
   */
  testcase.requestMountMultipleMounts = () => {
    const multipleMounts = true;
    return requestMountInternal(
        multipleMounts, 'manifest_multiple_mounts.json');
  };

  /**
   * Tests mounting a device not present in the button menu.
   */
  testcase.requestMountSourceDevice = () => {
    return requestMountNotInMenuInternal('manifest_source_device.json');
  };

  /**
   * Tests mounting a file not present in the button menu.
   */
  testcase.requestMountSourceFile = () => {
    return requestMountNotInMenuInternal('manifest_source_file.json');
  };

  /**
   * Tests that pressing the eject button on a FSP adds a message to screen
   * reader.
   */
  testcase.providerEject = async () => {
    const manifest = 'manifest_source_file.json';
    const appId = await setUpProvider(manifest);

    // Click to eject Test (1) provider/volume.
    const ejectQuery =
        ['#directory-tree [volume-type-for-testing="provided"] .root-eject'];
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId, ejectQuery),
        'click eject failed');

    // Wait a11y-msg to have some text.
    await remoteCall.waitForElement(appId, '#a11y-msg:not(:empty)');

    // Fetch A11y messages.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);

    // Check that opening the file was announced to screen reader.
    chrome.test.assertTrue(a11yMessages instanceof Array);
    chrome.test.assertEq(1, a11yMessages.length);
    chrome.test.assertEq('Test (1) has been ejected.', a11yMessages[0]);

    // Note: We disable app error checking because sometimes there are
    // JS errors due to volume related actions performed while volume is
    // ejected.
    return IGNORE_APP_ERRORS;
  };

  /**
   * Tests that when online, the install new service button is enabled.
   */
  testcase.installNewServiceOnline = async () => {
    const appId = await setUpProvider('manifest.json');
    await showProvidersMenu(appId);

    const selector = '#add-new-services-menu:not([hidden]) ' +
        'cr-menu-item[command="#install-new-extension"]:not([disabled])';
    const element = await remoteCall.waitForElement(appId, selector);
    chrome.test.assertEq('Install new service', element.text);
    chrome.test.assertFalse(element.hidden);
  };

  /**
   * Tests that when offline, the install new service button is disabled.
   */
  testcase.installNewServiceOffline = async () => {
    const appId = await setUpProvider('manifest.json');
    await showProvidersMenu(appId);

    const selector = '#add-new-services-menu:not([hidden]) ' +
        'cr-menu-item[command="#install-new-extension"][disabled]';
    const element = await remoteCall.waitForElement(appId, selector);
    chrome.test.assertEq('Install new service', element.text);
    chrome.test.assertFalse(element.hidden);
  };
})();
