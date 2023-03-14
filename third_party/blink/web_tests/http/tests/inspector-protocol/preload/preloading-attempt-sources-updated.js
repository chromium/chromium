(async function(testRunner) {
  async function backendNodeIdsToNodeIds(dp, backendNodeIds) {
    return (await dp.DOM.pushNodesByBackendIdsToFrontend({backendNodeIds})).result.nodeIds;
  }

  function compareAttempts(a, b) {
    if (a.key.action !== b.key.action) {
      return a.key.action < b.key.action ? -1: 1;
    }
    if (a.key.url !== b.key.url) {
      return a.key.url < b.key.url ? -1: 1;
    }
    return a.key.targetHint < b.key.targetHint ? -1: 1;
  }

  // Formats |preloadingAttemptSources| for logging by doing the following:
  // 1) Sorts the attempts based on the preloading attempt key (see
  //    compareAttempts above).
  // 2) Converts raw ruleSetIds into stable identifiers after verifying that
  //    they are in |validRuleSetIds|.
  // 3) Does the same with nodeIds and |validNodeIds|.
  // 4) Sorts ruleSetIds and nodeIds (after transforming them).
  async function formatPreloadingAttemptSources(dp,
                                                preloadingAttemptSources,
                                                validRuleSetIds,
                                                validNodeIds) {
    preloadingAttemptSources.sort(compareAttempts);
    for (const attempt of preloadingAttemptSources) {
      attempt.ruleSetIds = attempt.ruleSetIds.map(ruleSetId => {
        const index = validRuleSetIds.indexOf(ruleSetId);
        return index === -1 ? "<invalid-rule-set>" : `<rule-set-${index + 1}>`;
      }).sort();

      if (!attempt.nodeIds.length) {
        continue;
      }
      attempt.nodeIds = await backendNodeIdsToNodeIds(dp, attempt.nodeIds);
      attempt.nodeIds = attempt.nodeIds.map(nodeId => {
        const index = validNodeIds.indexOf(nodeId);
        return index === -1 ? "<unknown-node>" : `<link-${index+1}>`;
      }).sort();
    }
    return preloadingAttemptSources;
  }

  async function basicTest() {
    const { dp, page } = await testRunner.startBlank(
      `Tests that Preload.preloadingAttemptSourcesUpdated is dispatched.`);
    await dp.Preload.enable();

    page.loadHTML(`
      <html>
        <head>
          <script type="speculationrules">
            {"prefetch": [{
              "source": "list",
              "urls": ["/subresource.js", "/page.html"]
            }]}
          </script>
        </head>
        <body></body>
      </html>
    `);
    const preloadingAttemptSourcesUpdated = dp.Preload.oncePreloadingAttemptSourcesUpdated();
    const ruleSet = (await dp.Preload.onceRuleSetUpdated()).params.ruleSet;
    const preloadAttemptSources = (await preloadingAttemptSourcesUpdated).params.preloadingAttemptSources;
    testRunner.log(
      await formatPreloadingAttemptSources(dp, preloadAttemptSources, [ruleSet.id], []),
      "Preload attempts: ");
  }

  async function multipleRuleSetsWithDuplicates() {
    const { dp, page } = await testRunner.startBlank(
      `Tests that Preload.preloadingAttemptSourcesUpdated groups duplicates correctly.`);
    await dp.Preload.enable();

    page.loadHTML(`
      <html>
        <head>
          <script type="speculationrules">
            {"prefetch": [{
              "source": "list",
              "urls": ["/one.html", "/two.html"]
            }]}
          </script>
          <script type="speculationrules">
            {
              "prefetch": [{
                "source": "list",
                "urls": ["/one.html", "/three.html"]
              }],
              "prerender": [{
                "source": "list",
                "urls": ["/two.html"]
              }]
            }
          </script>
        </head>
        <body></body>
      </html>
    `);

    const preloadingAttemptSourcesUpdated = dp.Preload.oncePreloadingAttemptSourcesUpdated();
    const ruleSet1 = (await dp.Preload.onceRuleSetUpdated()).params.ruleSet;
    const ruleSet2 = (await dp.Preload.onceRuleSetUpdated()).params.ruleSet;
    const preloadAttemptSources = (await preloadingAttemptSourcesUpdated).params.preloadingAttemptSources;
    testRunner.log(
      await formatPreloadingAttemptSources(dp, preloadAttemptSources,
                                           [ruleSet1.id, ruleSet2.id], []),
      "Preload attempts: ");
  }

  async function documentRules() {
    const { dp, page } = await testRunner.startBlank(
      `Tests that Preload.preloadingAttemptSourcesUpdated is sent for document rule triggered attempts.`);
    await dp.Preload.enable();
    await dp.DOM.enable();

    page.loadHTML(`
      <html>
        <head>
          <script type="speculationrules">
            {"prefetch": [{
              "source": "document",
              "where": {"href_matches": "/t*"}
            }]}
          </script>
          <script type="speculationrules">
            {
              "prefetch": [{
                "source": "document",
                "where": {"selector_matches": ".less-important-links a"}
              }],
              "prerender": [{
                "source": "document",
                "where": {"selector_matches": ".important-links a"}
              }]
            }
          </script>
        </head>
        <body>
            <div class="important-links">
              <a href="/time.html"></a>
              <a href="/space.html"></a>
            </div>
            <div class="less-important-links">
              <a href="/universe.html"></a>
              <a href="/star.html"></a>
            </div>
            <div class="misc-links">
              <a href="/tau-ceti.html"></a>
              <a href="/sol.html"></a>
              <a href="/time.html"></a>
            </div>
        </body>
      </html>
    `);

    const preloadingAttemptSourcesUpdated = dp.Preload.oncePreloadingAttemptSourcesUpdated();
    const ruleSet1 = (await dp.Preload.onceRuleSetUpdated()).params.ruleSet;
    const ruleSet2 = (await dp.Preload.onceRuleSetUpdated()).params.ruleSet;
    const preloadAttemptSources = (await preloadingAttemptSourcesUpdated).params.preloadingAttemptSources;

    const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
    const importantLinks = (await dp.DOM.querySelectorAll({
      nodeId: documentNodeId, selector: ".important-links a"})).result.nodeIds;
    const lessImportantLinks = (await dp.DOM.querySelectorAll({
        nodeId: documentNodeId, selector: ".less-important-links a"
    })).result.nodeIds;
    const miscLinks = (await dp.DOM.querySelectorAll({
      nodeId: documentNodeId, selector: ".misc-links a"
    })).result.nodeIds;

    testRunner.log(
      await formatPreloadingAttemptSources(
        dp,
        preloadAttemptSources,
        [ruleSet1.id, ruleSet2.id],
        [...importantLinks, ...lessImportantLinks, ...miscLinks]),
      "Preload attempts: ");
  }

  async function duplicateRuleSets() {
    const { dp, page } = await testRunner.startBlank(
      `Tests that Preload.preloadingAttemptSourcesUpdated de-duplicates node ids when there are two identical rule sets.`);
    await dp.Preload.enable();
    await dp.DOM.enable();

    page.loadHTML(`
      <html>
        <head>
          <script type="speculationrules">
            {"prefetch": [{
              "source": "document"
            }]}
          </script>
          <script type="speculationrules">
            {"prefetch": [{
              "source": "document"
            }]}
          </script>
        </head>
        <body>
          <div class="links">
            <a href="/foo.html"></a>
            <a href="/bar.html"></a>
          </div>
        </body>
      </html>
    `);

    const preloadingAttemptSourcesUpdated = dp.Preload.oncePreloadingAttemptSourcesUpdated();
    const ruleSet1 = (await dp.Preload.onceRuleSetUpdated()).params.ruleSet;
    const ruleSet2 = (await dp.Preload.onceRuleSetUpdated()).params.ruleSet;
    const preloadAttemptSources = (await preloadingAttemptSourcesUpdated).params.preloadingAttemptSources;
    const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
    const links = (await dp.DOM.querySelectorAll({
      nodeId: documentNodeId, selector: ".links a"
    })).result.nodeIds;

    testRunner.log(
      await formatPreloadingAttemptSources(
        dp,
        preloadAttemptSources,
        [ruleSet1.id, ruleSet2.id],
        [...links]),
      "Preload attempts: "
    );
  }

  async function dynamicUpdate() {
    const { dp, session, page } = await testRunner.startBlank(
      `Tests that Preload.preloadingAttemptSourcesUpdated is dynamic.`);
    await dp.Preload.enable();
    await dp.DOM.enable();

    page.loadHTML(`
      <html>
        <head>
          <script type="speculationrules">
            {"prefetch": [{
              "source": "document"
            }]}
          </script>
        </head>
        <body>
        </body>
      </html>
    `);

    let preloadAttemptSources = (await dp.Preload.oncePreloadingAttemptSourcesUpdated()).params.preloadingAttemptSources;

    // There won't be any attempts at this point.
    testRunner.log(preloadAttemptSources, "Preloading attempts: ");

    session.evaluate(() => {
      const a = document.createElement('a');
      a.href = 'https://foo.com/bar.html';
      document.body.appendChild(a);
    });
    preloadAttemptSources = (await dp.Preload.oncePreloadingAttemptSourcesUpdated()).params.preloadingAttemptSources;

    testRunner.log(preloadAttemptSources, "Preloading attempts (after DOM mutation): ",
                   ["ruleSetIds", "nodeIds", "loaderId"]);
  }

  testRunner.runTestSuite([
    basicTest,
    multipleRuleSetsWithDuplicates,
    documentRules,
    duplicateRuleSets,
    dynamicUpdate,
  ]);
});
