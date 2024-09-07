// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const { dp, page } = await testRunner.startBlank(
    `Tests that Preload.preloadingAttemptSourcesUpdated and Preload.ruleSetUpdated is
     dispatched after the Preload domain is enabled.`);

  await page.loadHTML(`
    <html>
      <head>
        <script type="speculationrules">
          {"prefetch": [{
            "urls": ["/subresource.js"]
          }]}
        </script>
      </head>
    </html>
  `);

  const ruleSetUpdated = dp.Preload.onceRuleSetUpdated();
  const preloadingAttemptSourcesUpdated = dp.Preload.oncePreloadingAttemptSourcesUpdated();
  await dp.Preload.enable();

  const {ruleSet} = (await ruleSetUpdated).params;
  testRunner.log(`got rule set: ${ruleSet.sourceText}`);

  const {preloadingAttemptSources} = (await preloadingAttemptSourcesUpdated).params;
  testRunner.log(`got preloading attempt sources: ${preloadingAttemptSources.length}`);

  testRunner.completeTest();
});
