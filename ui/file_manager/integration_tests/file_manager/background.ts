// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is the entry point for the integration test.
 */

import {RemoteCallFilesApp} from '../remote_call.js';
import type {GetRootPathsResult} from '../test_util.js';
import {RootPath, sendBrowserTestCommand} from '../test_util.js';
import {testcase} from '../testcase.js';

/** Application ID (URL) for File Manager System Web App (SWA). */
const FILE_MANAGER_SWA_ID = 'chrome://file-manager';

export let remoteCall: RemoteCallFilesApp;

/**
 * For async function tests, wait for the test to complete, check for app errors
 * unless skipped, and report the results.
 * @param resultPromise A promise that resolves with the test result.
 */
async function awaitAsyncTestResult(resultPromise: Promise<void|any>) {
  chrome.test.assertTrue(
      resultPromise instanceof Promise, 'test did not return a Promise');

  try {
    await resultPromise;
  } catch (error: any) {
    // If the test has failed, ignore the exception and return.
    if (error === 'chrome.test.failure') {
      return;
    }

    // Otherwise, report the exception as a test failure. chrome.test.fail()
    // emits an exception; catch it to avoid spurious logging about an uncaught
    // exception.
    try {
      chrome.test.fail(error.stack || error);
    } catch (_) {
      return;
    }
  }

  chrome.test.succeed();
}

/**
 * When the FileManagerBrowserTest harness loads this test extension, request
 * configuration and other details from that harness, including the test case
 * name to run. Use the configuration/details to setup the test environment,
 * then run the test case using chrome.test.RunTests.
 */
window.addEventListener('load', async () => {
  // Request the guest mode state.
  remoteCall = new RemoteCallFilesApp(FILE_MANAGER_SWA_ID);
  const mode = await sendBrowserTestCommand({name: 'isInGuestMode'});

  // Request the root entry paths.
  if (JSON.parse(mode) !== chrome.extension.inIncognitoContext) {
    return;
  }
  const paths = await sendBrowserTestCommand({name: 'getRootPaths'});
  // Request the test case name.
  const roots: GetRootPathsResult = JSON.parse(paths);
  RootPath.DOWNLOADS = roots.downloads;
  RootPath.MY_FILES = roots.my_files;
  RootPath.DRIVE = roots.drive;
  RootPath.ANDROID_FILES = roots.android_files;
  const testCaseName = await sendBrowserTestCommand({name: 'getTestName'});

  // Get the test function from testcase namespace testCaseName.
  const test = testcase[testCaseName];
  // Verify test is a Function without args.
  if (!(test instanceof Function && test.length === 0)) {
    chrome.test.fail('[' + testCaseName + '] not found.');
  }
  // Define the test case and its name for chrome.test logging.
  const testCase = {
    [testCaseName]: () => {
      return awaitAsyncTestResult(test());
    },
  };

  // Run the test.
  chrome.test.runTests([testCase[testCaseName]!]);
});
