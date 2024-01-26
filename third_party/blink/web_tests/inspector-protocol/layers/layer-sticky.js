// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
  <style>
    .tall {
      height: 200vh;
    }
    .sticky {
      height: 50px;
      position: sticky;
      top: 0;
    }
  </style>

  <div class="sticky">
    <div class="sticky">
    </div>
  </div>
  <div class="tall">
  </div>
`, 'Tests nested sticky containers');

  await dp.LayerTree.enable();
  await dp.LayerTree.onceLayerTreeDidChange();

  testRunner.log('Didn\'t crash');
  testRunner.completeTest();
});
