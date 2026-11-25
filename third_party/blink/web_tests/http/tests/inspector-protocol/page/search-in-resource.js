// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests single resource search in inspector page agent.\n`);

  await dp.Page.enable();

  await session.evaluateAsync(`
    new Promise(resolve => {
      const iframe = document.createElement('iframe');
      iframe.src = '${testRunner.url('resources/search.html')}';
      iframe.onload = () => resolve();
      document.body.appendChild(iframe);
    })
  `);

  const {result: {frameTree}} = await dp.Page.getResourceTree();

  function findResource(tree, urlSuffix) {
    for (const resource of tree.resources) {
      if (resource.url.endsWith(urlSuffix)) {
        return {frameId: tree.frame.id, url: resource.url};
      }
    }
    for (const child of tree.childFrames || []) {
      const found = findResource(child, urlSuffix);
      if (found) {
        return found;
      }
    }
    return null;
  }

  const resource = findResource(frameTree, '/resources/search.js');
  if (!resource) {
    testRunner.fail('could not find search.js resource');
    return;
  }
  const {frameId, url} = resource;
  testRunner.log(url);

  function dumpSearchMatches(response) {
    if (response.error) {
      testRunner.fail(`searchInResource failed: ${response.error.message}`);
      return;
    }
    testRunner.log('Search matches:');
    const matches = response.result.result;
    if (matches.length === 0) {
      testRunner.log('No search matches');
    } else {
      for (const match of matches) {
        testRunner.log(
            `lineNumber: ${match.lineNumber}, line: '${match.lineContent}'`);
      }
    }
    testRunner.log('');
  }

  // This file should not match search query.
  const text = 'searchTest' +
      'UniqueString';
  dumpSearchMatches(await dp.Page.searchInResource({
    frameId,
    url,
    query: text,
    caseSensitive: false,
    isRegex: false,
  }));

  dumpSearchMatches(await dp.Page.searchInResource({
    frameId,
    url,
    query: text,
    caseSensitive: true,
    isRegex: false,
  }));

  const regexText = '[a-z]earchTestUniqueString';
  dumpSearchMatches(await dp.Page.searchInResource({
    frameId,
    url,
    query: regexText,
    caseSensitive: false,
    isRegex: true,
  }));

  dumpSearchMatches(await dp.Page.searchInResource({
    frameId,
    url,
    query: regexText,
    caseSensitive: true,
    isRegex: true,
  }));

  testRunner.completeTest();
});
