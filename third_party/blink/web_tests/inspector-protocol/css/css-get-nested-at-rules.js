(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
html {
  & body {
    margin: 10px;
  }
}

@media (min-width: 10px) {
  body {
      padding: 10px;
  }

  @supports (display: grid) {
    body {
      padding: 20px;
    }
  }
}

@supports (display: grid) {
  body {
    padding: 10px;
  }

  @media (min-width: 10px) {
    body {
      padding: 20px;
    }
  }

  @media (min-width: 2px) {
    @supports (--a: b) {
      @media (min-width: 1px) {
        @supports (display: flex) {
          body {
            background: pink;
          }
        }
      }
    }
  }
}
</style>
<body></body>
`, 'Verify that nested at-rules are reported with the correct order.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;
  const body = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: 'body',
  });
  const bodyId = body.result.nodeId;

  const matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: bodyId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    if (ruleMatch.rule.ruleTypes.length > 0) {
      testRunner.log(ruleMatch.rule.ruleTypes);
    }
    if (ruleMatch.rule.nestingSelectors) {
      testRunner.log(ruleMatch.rule.nestingSelectors);
    }
  }

  testRunner.completeTest();
});
