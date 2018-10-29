// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const crostiniMount = {};

crostiniMount.testCrostiniNotEnabled = (done) => {
  chrome.fileManagerPrivate.crostiniEnabled_ = false;
  test.setupAndWaitUntilReady()
      .then(() => {
        fileManager.setupCrostini_();
        return test.waitForElementLost(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Reset crostini back to default enabled=true.
        chrome.fileManagerPrivate.crostiniEnabled_ = true;
        done();
      });
};

crostiniMount.testMountCrostiniSuccess = (done) => {
  const oldMount = chrome.fileManagerPrivate.mountCrostini;
  let mountCallback = null;
  chrome.fileManagerPrivate.mountCrostini = (callback) => {
    mountCallback = callback;
  };
  test.setupAndWaitUntilReady()
      .then(() => {
        // Linux files fake root is shown.
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Click on Linux files.
        assertTrue(
            test.fakeMouseClick(
                '#directory-tree .tree-item [root-type-icon="crostini"]'),
            'click linux files');
        return test.waitForElement('paper-progress:not([hidden])');
      })
      .then(() => {
        // Ensure mountCrostini is called.
        return test.repeatUntil(() => {
          if (!mountCallback)
            return test.pending('Waiting for mountCrostini');
          return mountCallback;
        });
      })
      .then(() => {
        // Intercept the fileManagerPrivate.mountCrostini call
        // and add crostini disk mount.
        test.mountCrostini();
        // Continue from fileManagerPrivate.mountCrostini callback
        // and ensure expected files are shown.
        mountCallback();
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        // Reset fileManagerPrivate.mountCrostini and remove mount.
        chrome.fileManagerPrivate.mountCrostini = oldMount;
        chrome.fileManagerPrivate.removeMount('crostini');
        // Linux Files fake root is shown.
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Downloads folder should be shown when crostini goes away.
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_LOCAL_ENTRY_SET));
      })
      .then(() => {
        done();
      });
};

crostiniMount.testMountCrostiniError = (done) => {
  const oldMount = chrome.fileManagerPrivate.mountCrostini;
  // Override fileManagerPrivate.mountCrostini to return error.
  chrome.fileManagerPrivate.mountCrostini = (callback) => {
    chrome.runtime.lastError = {message: 'test message'};
    callback();
    delete chrome.runtime.lastError;
  };
  test.setupAndWaitUntilReady()
      .then(() => {
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Click on Linux Files, ensure error dialog is shown.
        assertTrue(test.fakeMouseClick(
            '#directory-tree .tree-item [root-type-icon="crostini"]'));
        return test.waitForElement('.cr-dialog-container.shown');
      })
      .then(() => {
        // Click OK button to close.
        assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
        return test.waitForElementLost('.cr-dialog-container');
      })
      .then(() => {
        // Reset chrome.fileManagerPrivate.mountCrostini.
        chrome.fileManagerPrivate.mountCrostini = oldMount;
        done();
      });
};

crostiniMount.testCrostiniMountOnDrag = (done) => {
  chrome.fileManagerPrivate.mountCrostiniDelay_ = 0;
  test.setupAndWaitUntilReady()
      .then(() => {
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        assertTrue(test.sendEvent(
            '#directory-tree .tree-item [root-type-icon="crostini"]',
            new Event('dragenter', {bubbles: true})));
        assertTrue(test.sendEvent(
            '#directory-tree .tree-item [root-type-icon="crostini"]',
            new Event('dragleave', {bubbles: true})));
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        chrome.fileManagerPrivate.removeMount('crostini');
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        done();
      });
};
