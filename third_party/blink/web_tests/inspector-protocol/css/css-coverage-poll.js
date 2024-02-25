(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('./resources/coverage.css')}'>
<h1 class='class'>Class Selector</h1>
<p id='id' class='usedStraightAway'>ID Selector</p>
<div></div>
`, 'Test css coverage.');

  var stylesheetIdToURL = new Map();
  dp.CSS.onStyleSheetAdded(event => {
    var header = event.params.header;
    var url = /(([^/]*\/){2}[^/]*$)/.exec(header.sourceURL)[1];
    stylesheetIdToURL.set(header.styleSheetId, url);
  });

  await dp.DOM.enable();
  await dp.CSS.enable();
  await dp.CSS.startRuleUsageTracking();

  testRunner.log('\nInitial coverage:');
  dumpCoverageData((await dp.CSS.takeCoverageDelta()).result.coverage);

  testRunner.log('\nLoad stylesheet coverage2.css:');
  await session.evaluateAsync(function(url) {
    var link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = url;
    var promise = new Promise(fulfill => {
      link.addEventListener('load', requestAnimationFrame.bind(window, fulfill));
    });
    document.head.appendChild(link);
    return promise;
  }, testRunner.url('./resources/coverage2.css'));
  dumpCoverageData((await dp.CSS.takeCoverageDelta()).result.coverage);

  await useClass('usedSomewhatLater');
  dumpCoverageData((await dp.CSS.takeCoverageDelta()).result.coverage);

  await useClass('usedAtTheVeryEnd');
  dumpCoverageData((await dp.CSS.takeCoverageDelta()).result.coverage);

  var response = await dp.CSS.stopRuleUsageTracking();
  if (response.result.ruleUsage.length)
    dumpCoverageData(response.result.ruleUsage);

  testRunner.completeTest();

  async function useClass(className) {
    testRunner.log(`\nUse class: "${className}"`);
    await session.evaluateAsync(function(className) {
      var div = document.createElement('div');
      div.classList.add(className);
      document.body.appendChild(div);
      div.offsetHeight; // Force layout & style recalc
      return Promise.resolve();
    }, className);
  }

  function dumpCoverageData(rules) {
    testRunner.log(`Coverage delta (${rules.length} entries)`);
    rules.sort((a, b) =>
        (stylesheetIdToURL.get(a.styleSheetId) || '').localeCompare(stylesheetIdToURL.get(b.styleSheetId)) || a.startOffset - b.startOffset
    );
    var lastURL;
    var output = '';
    for (var rule of rules) {
      var url = stylesheetIdToURL.get(rule.styleSheetId) || '<unknown>';
      if (lastURL !== url)
        output += `    ${output ? '\n' : ''}${url}:`;
      lastURL = url;
      var used = rule.used ? '+' : '-';
      output += ` ${used}${rule.startOffset}-${rule.endOffset}`;
    }
    testRunner.log(output);
  }
});
