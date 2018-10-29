// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/**
 * Files app windowId.
 */
var appId;

/**
 * Returns provider name of the given testing provider manifest viz., the
 * the value of the name field in the |manifest| file.
 * @param {string} manifest Testing provider manifest file name.
 * @return {string} Testing provider name.
 */
function getProviderNameForTest(manifest) {
  if (manifest === 'manifest.json')
    return 'Files Testing Provider test extension';
  if (manifest === 'manifest_multiple_mounts.json')
    return 'Files Testing Provider multiple mounts test extension';
  if (manifest === 'manifest_source_device.json')
    return 'Files Testing Provider device test extension';
  if (manifest === 'manifest_source_file.json')
    return 'Files Testing Provider file test extension';

  throw new Error('unknown mainfest: '.concat(manifest));
}

/**
 * Returns steps for initializing test cases.
 * @param {string} manifest The manifest name of testing provider extension
 *     to launch for the test case.
 * @return {!Array<function>}
 */
function getSetupSteps(manifest) {
  return [
    function() {
      sendTestMessage({
          name: 'launchProviderExtension',
          manifest: manifest,
      }).then(this.next);
    },
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    function(results) {
      appId = results.windowId;
      this.next();
    }
  ];
}

/**
 * Returns steps for clicking on the "gear menu".
 * @return {!Array<function>}
 */
function clickGearMenu() {
  const newServiceMenuItem = '#gear-menu-newservice:not([hidden])';
  return [
    // Open the gear menu by clicking the gear button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for Add new service menu item to appear in the gear menu.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      remoteCall.waitForElement(appId, newServiceMenuItem).then(this.next);
    },
  ];
}

/**
 * Returns steps for clicking on the "Add new services" menu button.
 * @return {!Array<function>}
 */
function showProvidersMenuSteps() {
  const newServiceMenuItem = '#gear-menu-newservice:not([hidden])';
  return [
    // Open the gear menu by clicking the gear button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for Add new service menu item to appear.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      remoteCall.waitForElement(appId, newServiceMenuItem).then(this.next);
    },
    // Click the menu item.
    function(result) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [newServiceMenuItem], this.next);
    },
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      this.next();
    },
  ];
}

/**
 * Returns steps for confirming that a provided volume is mounted.
 * @return {!Array<function>}
 */
function getConfirmVolumeSteps(ejectExpected) {
  return [
    function() {
      remoteCall.waitForElement(
          appId,
          '.tree-row .icon[volume-type-icon="provided"]')
          .then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick',
          appId,
          ['.tree-row .icon[volume-type-icon="provided"]'],
          this.next);
    },
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      remoteCall.waitForElement(
          appId,
          '.tree-row[selected] .icon[volume-type-icon="provided"]')
          .then(this.next);
    },
    function() {
      if (ejectExpected) {
        remoteCall.waitForElement(
            appId,
            '.tree-row[selected] .root-eject')
            .then(this.next);
      } else {
        remoteCall.waitForElementLost(
            appId,
            '.tree-row[selected] .root-eject')
            .then(this.next);
      }
    },
  ];
}

/**
 * Tests that a provided extension with |manifest| is mountable via the menu
 * button.
 *
 * @param {boolean} multipleMounts Whether multiple mounts are supported by the
 *     providing extension.
 * @param {string} manifest Name of the manifest file for the providing
 *     extension.
 */
function requestMountInternal(multipleMounts, manifest) {
  const providerName = getProviderNameForTest(manifest);

  StepsRunner.runGroups([
    getSetupSteps(manifest),
    showProvidersMenuSteps(),
    [
      // Wait for providers menu and new service menu item to appear.
      function() {
        remoteCall.waitForElement(
            appId,
            '#add-new-services-menu:not([hidden]) cr-menu-item:first-child ' +
                'span')
            .then(this.next);
      },
      // Click to install test provider.
      function(result) {
        chrome.test.assertEq(providerName, result.text);
        remoteCall.callRemoteTestUtil(
            'fakeMouseClick',
            appId,
            ['#add-new-services-menu cr-menu-item:first-child span'],
            this.next);
      },
      function(result) {
        chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
        this.next();
      },
    ],
    getConfirmVolumeSteps(false /* ejectExpected */),
    // If multipleMounts we display the providers menu, otherwise we display the
    // gear menu and check the "add new service" menu item.
    multipleMounts ? showProvidersMenuSteps() : clickGearMenu(),
    [
      function() {
        // When multiple mounts are supported, then the "new service" menu item
        // should open the providers menu. Otherwise it should go directly to
        // install-new-extension, however install-new-service command uses
        // webview which doesn't work in the integration tests.
        var selector = multipleMounts ?
            '#add-new-services-menu:not([hidden]) cr-menu-item:first-child ' +
                'span' :
            '#gear-menu:not([hidden]) ' +
                'cr-menu-item[command="#install-new-extension"]';
        remoteCall.waitForElement(
            appId,
            selector)
            .then(this.next);
      },
      function(result) {
        if (multipleMounts)
          chrome.test.assertEq(providerName, result.text);
        checkIfNoErrorsOccured(this.next);
      }
    ]
  ]);
}

/**
 * Tests that a provided extension with |manifest| is not available in the
 * button menu, but it's mounted automatically.
 *
 * @param {string} manifest Name of the manifest file for the providing
 *     extension.
 */
function requestMountNotInMenuInternal(manifest) {
  StepsRunner.runGroups([
    getSetupSteps(manifest),
    getConfirmVolumeSteps(true /* ejectExpected */),
    clickGearMenu(),
    [
      function(element) {
        // clickGearMenu returns the "Add new service" menu item.
        // Here we only check these attributes because the menu item calls
        // Webstore using webview which doesn't work in the integration test.
        chrome.test.assertEq('Install new service', element.text);
        chrome.test.assertEq(
            '#install-new-extension', element.attributes.command);
        this.next();
      },
      function() {
        checkIfNoErrorsOccured(this.next);
      }
    ]
  ]);
}

/**
 * Tests mounting a single mount point in the button menu.
 */
testcase.requestMount = function() {
  const multipleMounts = false;
  requestMountInternal(multipleMounts, 'manifest.json');
};

/**
 * Tests mounting multiple mount points in the button menu.
 */
testcase.requestMountMultipleMounts = function() {
  const multipleMounts = true;
  requestMountInternal(multipleMounts, 'manifest_multiple_mounts.json');
};

/**
 * Tests mounting a device not present in the button menu.
 */
testcase.requestMountSourceDevice = function() {
  requestMountNotInMenuInternal('manifest_source_device.json');
};

/**
 * Tests mounting a file not present in the button menu.
 */
testcase.requestMountSourceFile = function() {
  requestMountNotInMenuInternal('manifest_source_file.json');
};

})();
