// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Returns an array of steps that opens the Quick View dialog on a given file
 * |name|. The file must be present in the Files app file list.
 *
 * @param {string} appId Files app windowId.
 * @param {string} name File name.
 * @return {!Array<function>}
 */
function openQuickViewSteps(appId, name) {
  let caller = getCaller();

  function checkQuickViewElementsDisplayBlock(elements) {
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0].styles.display !== 'block')
      return pending(caller, 'Waiting for Quick View to open.');
    return;
  }

  return [
    // Select file |name| in the file list.
    function() {
      remoteCall.callRemoteTestUtil('selectFile', appId, [name], this.next);
    },
    // Press the space key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const space = ['#file-list', ' ', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space, this.next);
    },
    // Check: the Quick View element should be shown.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      repeatUntil(function() {
        const elements = ['#quick-view', '#dialog[open]'];
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [elements, ['display']])
            .then(checkQuickViewElementsDisplayBlock);
      }).then(this.next);
    },
  ];
}

/**
 * Assuming that Quick View is currently open per the openQuickViewSteps above,
 * returns an array of steps that closes the Quick View dialog.
 *
 * @param {string} appId Files app windowId.
 * @return {!Array<function>}
 */
function closeQuickViewSteps(appId) {
  let caller = getCaller();

  function checkQuickViewElementsDisplayNone(elements) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length > 0 && elements[0].styles.display !== 'none')
      return pending(caller, 'Waiting for Quick View to close.');
    return;
  }

  return [
    // Click on Quick View to close it.
    function() {
      const panelElements = ['#quick-view', '#contentPanel'];
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [panelElements])
          .then(this.next);
    },
    // Check: the Quick View element should not be shown.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      repeatUntil(function() {
        const elements = ['#quick-view', '#dialog:not([open])'];
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [elements, ['display']])
            .then(checkQuickViewElementsDisplayNone);
      }).then(this.next);
    },
  ];
}

/**
 * Tests opening Quick View on a local downloads file.
 */
testcase.openQuickView = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.hello.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.hello], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening then closing Quick View on a local downloads file.
 */
testcase.closeQuickView = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.hello.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.hello], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Close Quick View.
    function() {
      StepsRunner.run(closeQuickViewSteps(appId)).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on a Drive file.
 */
testcase.openQuickViewDrive = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive containing ENTRIES.hello.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], [ENTRIES.hello]);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on a USB file.
 */
testcase.openQuickViewUsb = function() {
  let appId;

  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.photos.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.photos], []);
    },
    // Mount a USB volume.
    function(results) {
      appId = results.windowId;
      sendTestMessage({name: 'mountFakeUsb'}).then(this.next);
    },
    // Wait for the USB volume to mount.
    function() {
      remoteCall.waitForElement(appId, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY], this.next);
    },
    // Check: the USB files should appear in the file list.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Open a USB file in Quick View.
    function() {
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on an MTP file.
 */
testcase.openQuickViewMtp = function() {
  let appId;

  const MTP_VOLUME_QUERY = '#directory-tree [volume-type-icon="mtp"]';

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.photos.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.photos], []);
    },
    // Mount a non-empty MTP volume.
    function(results) {
      appId = results.windowId;
      sendTestMessage({name: 'mountFakeMtp'}).then(this.next);
    },
    // Wait for the MTP volume to mount.
    function() {
      remoteCall.waitForElement(appId, MTP_VOLUME_QUERY).then(this.next);
    },
    // Click to open the MTP volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [MTP_VOLUME_QUERY], this.next);
    },
    // Check: the MTP files should appear in the file list.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Open an MTP file in Quick View.
    function() {
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on a Crostini file.
 */
testcase.openQuickViewCrostini = function() {
  let appId;

  const fakeLinuxFiles = '#directory-tree [root-type-icon="crostini"]';
  const realLinuxFiles = '#directory-tree [volume-type-icon="crostini"]';

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.photos.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.photos], []);
    },
    // Check: the fake Linux files icon should be shown.
    function(results) {
      appId = results.windowId;
      remoteCall.waitForElement(appId, fakeLinuxFiles).then(this.next);
    },
    // Add files to the Crostini volume.
    function() {
      addEntries(['crostini'], BASIC_CROSTINI_ENTRY_SET, this.next);
    },
    // Click the fake Linux files icon to mount the Crostini volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [fakeLinuxFiles], this.next);
    },
    // Check: the Crostini volume icon should appear.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      remoteCall.waitForElement(appId, realLinuxFiles).then(this.next);
    },
    // Check: the Crostini files should appear in the file list.
    function() {
      const files = TestEntryInfo.getExpectedRows(BASIC_CROSTINI_ENTRY_SET);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Open a Crostini file in Quick View.
    function() {
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on an Android file.
 */
testcase.openQuickViewAndroid = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Android files.
    function() {
      openNewWindow(null, RootPath.ANDROID_FILES).then(this.next);
    },
    // Add files to the Android files volume.
    function(result) {
      appId = result;
      const entrySet = BASIC_ANDROID_ENTRY_SET.concat([ENTRIES.documentsText]);
      addEntries(['android_files'], entrySet, this.next);
    },
    // Wait for the file list to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-list').then(this.next);
    },
    // Check: the basic Android file set should appear in the file list.
    function() {
      const files = TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Navigate to the Android files '/Documents' directory.
    function() {
      remoteCall
          .navigateWithDirectoryTree(
              appId, '/Documents', 'My files/Play files', 'android_files')
          .then(this.next);
    },
    // Check: the 'android.txt' file should appear in the file list.
    function() {
      const files = [ENTRIES.documentsText.getExpectedRow()];
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Open the Android file in Quick View.
    function() {
      const documentsFileName = ENTRIES.documentsText.nameText;
      const openSteps = openQuickViewSteps(appId, documentsFileName);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View and scrolling its <webview> which contains a tall
 * text document.
 */
testcase.openQuickViewScrollText = function() {
  const caller = getCaller();
  let appId;

  /**
   * The text <webview> resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const webView = ['#quick-view', '#dialog[open] webview.text-content'];

  function scrollQuickViewTextBy(y) {
    const doScrollBy = `window.scrollBy(0,${y})`;
    return remoteCall
        .callRemoteTestUtil(
            'deepExecuteScriptInWebView', appId, [webView, doScrollBy]);
  }

  function checkQuickViewTextScrollY(scrollY) {
    if (!scrollY || Number(scrollY.toString()) <= 200) {
      console.log('checkQuickViewTextScrollY: scrollY '.concat(scrollY));
      return scrollQuickViewTextBy(100).then(() => {
        return pending(caller, 'Waiting for Quick View to scroll.');
      });
    }
  }

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.tallText.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.tallText], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.tallText.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Wait for the Quick View <webview> to load and display its content.
    function() {
      function checkWebViewTextLoaded(elements) {
        let haveElements = Array.isArray(elements) && elements.length === 1;
        if (haveElements)
          haveElements = elements[0].styles.display.includes('block');
        if (!haveElements || !elements[0].attributes.src)
          return pending(caller, 'Waiting for <webview> to load.');
        return;
      }
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [webView, ['display']])
            .then(checkWebViewTextLoaded);
      }).then(this.next);
    },
    // Get the Quick View <webview> scrollY.
    function() {
      const getScrollY = 'window.scrollY';
      remoteCall
          .callRemoteTestUtil(
              'deepExecuteScriptInWebView', appId, [webView, getScrollY])
          .then(this.next);
    },
    // Check: the initial <webview> scrollY should be 0.
    function(scrollY) {
      chrome.test.assertEq('0', scrollY.toString());
      this.next();
    },
    // Scroll the <webview> and verify that it scrolled.
    function() {
      repeatUntil(function() {
        const getScrollY = 'window.scrollY';
        return remoteCall
          .callRemoteTestUtil(
              'deepExecuteScriptInWebView', appId, [webView, getScrollY])
          .then(checkQuickViewTextScrollY);
      }).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on a text document to verify that the background
 * color of the <webview> root (html) element is solid white.
 */
testcase.openQuickViewBackgroundColorText = function() {
  const caller = getCaller();
  let appId;

  /**
   * The text <webview> resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const webView = ['#quick-view', '#dialog[open] webview.text-content'];

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.tallText.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.tallText], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.tallText.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Wait for the Quick View <webview> to load and display its content.
    function() {
      function checkWebViewTextLoaded(elements) {
        let haveElements = Array.isArray(elements) && elements.length === 1;
        if (haveElements)
          haveElements = elements[0].styles.display.includes('block');
        if (!haveElements || !elements[0].attributes.src)
          return pending(caller, 'Waiting for <webview> to load.');
        return;
      }
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [webView, ['display']])
            .then(checkWebViewTextLoaded);
      }).then(this.next);
    },
    // Get the <webview> root (html) element backgroundColor style.
    function() {
      const getBackgroundStyle =
          'window.getComputedStyle(document.documentElement).backgroundColor';
      remoteCall
        .callRemoteTestUtil(
            'deepExecuteScriptInWebView', appId, [webView, getBackgroundStyle])
        .then(this.next);
    },
    // Check: the <webview> root backgroundColor should be solid white.
    function(backgroundColor) {
      chrome.test.assertEq('rgb(255, 255, 255)', backgroundColor[0]);
      this.next();
    },
    function(results) {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View containing a PDF document.
 */
testcase.openQuickViewPdf = function() {
  const caller = getCaller();
  let appId;

  /**
   * The PDF <webview> resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const webView = ['#quick-view', '#dialog[open] webview.content'];

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.tallPdf.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.tallPdf], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.tallPdf.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Wait for the Quick View <webview> to load and display its content.
    function() {
      function checkWebViewPdfLoaded(elements) {
        let haveElements = Array.isArray(elements) && elements.length === 1;
        if (haveElements)
          haveElements = elements[0].styles.display.includes('block');
        if (!haveElements || !elements[0].attributes.src)
          return pending(caller, 'Waiting for <webview> to load.');
        return;
      }
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [webView, ['display']])
            .then(checkWebViewPdfLoaded);
      }).then(this.next);
    },
    // Get the <webview> embed type attribute.
    function() {
      function checkPdfEmbedType(type) {
        let haveElements = Array.isArray(type) && type.length === 1;
        if (!haveElements || !type[0].toString().includes('pdf'))
          return pending(caller, 'Waiting for plugin <embed> type.');
        return type[0];
      }
      repeatUntil(function() {
        const getType = 'window.document.querySelector("embed").type';
        return remoteCall
            .callRemoteTestUtil(
                'deepExecuteScriptInWebView', appId, [webView, getType])
            .then(checkPdfEmbedType);
      }).then(this.next);
    },
    // Check: the <webview> embed type should be PDF mime type.
    function(type) {
      chrome.test.assertEq('application/pdf', type);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View and scrolling its <webview> which contains a tall
 * html document.
 */
testcase.openQuickViewScrollHtml = function() {
  const caller = getCaller();
  let appId;

  /**
   * The <webview> resides in the <files-safe-media type="html"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const webView = ['#quick-view', 'files-safe-media[type="html"]', 'webview'];

  function scrollQuickViewHtmlBy(y) {
    const doScrollBy = `window.scrollBy(0,${y})`;
    return remoteCall
        .callRemoteTestUtil(
            'deepExecuteScriptInWebView', appId, [webView, doScrollBy]);
  }

  function checkQuickViewHtmlScrollY(scrollY) {
    if (!scrollY || Number(scrollY.toString()) <= 200) {
      console.log('checkQuickViewHtmlScrollY: scrollY '.concat(scrollY));
      return scrollQuickViewHtmlBy(100).then(() => {
        return pending(caller, 'Waiting for Quick View to scroll.');
      });
    }
  }

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.tallHtml.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.tallHtml], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.tallHtml.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Wait for the Quick View <webview> to load and display its content.
    function() {
      function checkWebViewHtmlLoaded(elements) {
        let haveElements = Array.isArray(elements) && elements.length === 1;
        if (haveElements)
          haveElements = elements[0].styles.display.includes('block');
        if (!haveElements || elements[0].attributes.loaded !== '')
          return pending(caller, 'Waiting for <webview> to load.');
        return;
      }
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [webView, ['display']])
            .then(checkWebViewHtmlLoaded);
      }).then(this.next);
    },
    // Get the Quick View <webview> scrollY.
    function() {
      const getScrollY = 'window.scrollY';
      remoteCall
          .callRemoteTestUtil(
              'deepExecuteScriptInWebView', appId, [webView, getScrollY])
          .then(this.next);
    },
    // Check: the initial <webview> scrollY should be 0.
    function(scrollY) {
      chrome.test.assertEq('0', scrollY.toString());
      this.next();
    },
    // Scroll the <webview> and verify that it scrolled.
    function() {
      repeatUntil(function() {
        const getScrollY = 'window.scrollY';
        return remoteCall
          .callRemoteTestUtil(
              'deepExecuteScriptInWebView', appId, [webView, getScrollY])
          .then(checkQuickViewHtmlScrollY);
      }).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on an html document to verify that the background
 * color of the <files-safe-media type="html"> that contains the <webview> is
 * solid white.
 */
testcase.openQuickViewBackgroundColorHtml = function() {
  const caller = getCaller();
  let appId;

  /**
   * The <webview> resides in the <files-safe-media type="html"> shadow DOM,
   * which is a child of the #quick-view shadow DOM. This test only needs to
   * examine the <files-safe-media> element.
   */
  const fileSafeMedia = ['#quick-view', 'files-safe-media[type="html"]'];

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.tallHtml.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.tallHtml], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.tallHtml.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Get the <files-safe-media type='html'> backgroundColor style.
    function() {
      function getFileSafeMediaBackgroundColor(elements) {
        let haveElements = Array.isArray(elements) && elements.length === 1;
        if (haveElements)
          haveElements = elements[0].styles.display.includes('block');
        if (!haveElements || !elements[0].styles.backgroundColor)
          return pending(caller, 'Waiting for <file-safe-media> element.');
        return elements[0].styles.backgroundColor;
      }
      repeatUntil(function() {
        const styles = ['display', 'backgroundColor'];
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [fileSafeMedia, styles])
            .then(getFileSafeMediaBackgroundColor);
      }).then(this.next);
    },
    // Check: the <files-safe-media> backgroundColor should be solid white.
    function(backgroundColor) {
      chrome.test.assertEq('rgb(255, 255, 255)', backgroundColor);
      this.next();
    },
    function(results) {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View containing an audio file.
 */
testcase.openQuickViewAudio = function() {
  const caller = getCaller();
  let appId;

  /**
   * The <webview> resides in the <files-safe-media type="audio"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const webView = ['#quick-view', 'files-safe-media[type="audio"]', 'webview'];

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.beautiful song.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.beautiful], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.beautiful.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Wait for the Quick View <webview> to load and display its content.
    function() {
     function checkWebViewAudioLoaded(elements) {
        let haveElements = Array.isArray(elements) && elements.length === 1;
        if (haveElements)
          haveElements = elements[0].styles.display.includes('block');
        if (!haveElements || elements[0].attributes.loaded !== '')
          return pending(caller, 'Waiting for <webview> to load.');
        return;
      }
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [webView, ['display']])
            .then(checkWebViewAudioLoaded);
      }).then(this.next);
    },
    // Get the <webview> document.body backgroundColor style.
    function() {
      const getBackgroundStyle =
          'window.getComputedStyle(document.body).backgroundColor';
      remoteCall
        .callRemoteTestUtil(
            'deepExecuteScriptInWebView', appId, [webView, getBackgroundStyle])
        .then(this.next);
    },
    // Check: the <webview> body backgroundColor should be transparent black.
    function(backgroundColor) {
      chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View containing an image.
 */
testcase.openQuickViewImage = function() {
  const caller = getCaller();
  let appId;

  /**
   * The <webview> resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const webView = ['#quick-view', 'files-safe-media[type="image"]', 'webview'];

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.smallJpeg.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.smallJpeg], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.smallJpeg.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Wait for the Quick View <webview> to load and display its content.
    function() {
      function checkWebViewImageLoaded(elements) {
        let haveElements = Array.isArray(elements) && elements.length === 1;
        if (haveElements)
          haveElements = elements[0].styles.display.includes('block');
        if (!haveElements || elements[0].attributes.loaded !== '')
          return pending(caller, 'Waiting for <webview> to load.');
        return;
      }
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [webView, ['display']])
            .then(checkWebViewImageLoaded);
      }).then(this.next);
    },
    // Get the <webview> document.body backgroundColor style.
    function() {
      const getBackgroundStyle =
          'window.getComputedStyle(document.body).backgroundColor';
      remoteCall
        .callRemoteTestUtil(
            'deepExecuteScriptInWebView', appId, [webView, getBackgroundStyle])
        .then(this.next);
    },
    // Check: the <webview> body backgroundColor should be transparent black.
    function(backgroundColor) {
      chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View containing a video.
 */
testcase.openQuickViewVideo = function() {
  const caller = getCaller();
  let appId;

  /**
   * The <webview> resides in the <files-safe-media type="video"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const webView = ['#quick-view', 'files-safe-media[type="video"]', 'webview'];

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.world video.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.world], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.world.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Wait for the Quick View <webview> to load and display its content.
    function() {
     function checkWebViewVideoLoaded(elements) {
        let haveElements = Array.isArray(elements) && elements.length === 1;
        if (haveElements)
          haveElements = elements[0].styles.display.includes('block');
        if (!haveElements || elements[0].attributes.loaded !== '')
          return pending(caller, 'Waiting for <webview> to load.');
        return;
      }
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [webView, ['display']])
            .then(checkWebViewVideoLoaded);
      }).then(this.next);
    },
    // Get the <webview> document.body backgroundColor style.
    function() {
      const getBackgroundStyle =
          'window.getComputedStyle(document.body).backgroundColor';
      remoteCall
        .callRemoteTestUtil(
            'deepExecuteScriptInWebView', appId, [webView, getBackgroundStyle])
        .then(this.next);
    },
    // Check: the <webview> body backgroundColor should be transparent black.
    function(backgroundColor) {
      chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
