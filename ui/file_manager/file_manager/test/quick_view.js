// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const quickview = {};

/**
 * Helper function to open and close Quick View.
 * @param {string} file file to open and close.
 * @param {function(!Element)=} opt_validate optional validation function that
 *   receives the QuickView element as argument.
 */
quickview.openCloseQuickView = (file, opt_validate) => {
  // Using an image file for testing https://crbug.com/845830.
  // If this test starts to take too long on the bots, the image could be
  // changed to text file 'hello.txt'.
  assertTrue(test.selectFile(file));
  // Press Space key.
  assertTrue(test.fakeKeyDown('#file-list', ' ', false, false, false));
  // Wait until Quick View is displayed and files-safe-media.src is set.
  return test
      .repeatUntil(() => {
        let element = document.querySelector('#quick-view');
        if (element && element.shadowRoot) {
          element = element.shadowRoot.querySelector('#dialog');
          if (getComputedStyle(element).display === 'block' &&
              element.querySelector('files-safe-media').src)
            return element;
        }
        return test.pending('Quick View is not opened yet.');
      })
      .then((result) => {
        // Run optional validate.
        if (opt_validate)
          opt_validate(result);

        // Click panel and wait for close.
        assertTrue(test.fakeMouseClick(['#quick-view', '#contentPanel']));
        return test.repeatUntil(() => {
          if (getComputedStyle(result).display === 'none')
            return result;
          return test.pending('Quick View is not closed yet.');
        });
      });
};

/**
 * Tests opening Quick View for downloads.
 */
quickview.testOpenCloseQuickViewDownloads = (done) => {
  test.setupAndWaitUntilReady()
      .then(() => {
        return quickview.openCloseQuickView('My Desktop Background.png');
      })
      .then(() => {
        // Add hello.mhtml file and verify background is white.
        const entriesWithMhtml =
            test.BASIC_LOCAL_ENTRY_SET.concat([test.ENTRIES.mhtml]);
        test.addEntries(entriesWithMhtml, [], []);
        assertTrue(test.fakeMouseClick('#refresh-button'), 'click refresh');
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(entriesWithMhtml));
      })
      .then(() => {
        return quickview.openCloseQuickView('hello.mhtml', (qv) => {
          const htmlPanel = qv.querySelector(
              '#innerContentPanel files-safe-media[type="html"]');
          const style = window.getComputedStyle(htmlPanel);
          // White background is 'rgb(255, 255, 255)'.
          assertEquals('rgb(255, 255, 255)', style.backgroundColor, 'bg white');
        });
      })
      .then(() => {
        done();
      });
};

/**
 * Tests opening Quick View for crostini.
 */
quickview.testOpenCloseQuickViewCrostini = (done) => {
  test.setupAndWaitUntilReady()
      .then(() => {
        test.mountCrostini();
        return test.waitForElement(
            '#directory-tree [volume-type-icon="crostini"]');
      })
      .then(() => {
        assertTrue(test.fakeMouseClick(
            '#directory-tree [volume-type-icon="crostini"]'));
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        return quickview.openCloseQuickView('My Desktop Background.png');
      })
      .then(() => {
        chrome.fileManagerPrivate.removeMount('crostini');
        done();
      });
};
