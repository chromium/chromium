(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner
          .startHTML(
              `
  <style>
    #main{
    --a /* color: comment0 */ :/* color: comment1 */blue/* color: comment2 */;
    --b: /* color: comment0 */ blue;
    --c/: * blue */;
    --d/*: blue;
    --e/*: blue;
    --f/*:;
    --g/;
    --h/*:*/;
    --j/*:*;
    }
  </style>
  <div id=main>content</div>
      `,
              'Tests that the parser correctly splits a property declaration in the presence of comments')

              await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});
  const {result: {matchedCSSRules}} =
      await dp.CSS.getMatchedStylesForNode({nodeId});

  const rule =
      matchedCSSRules.find(({rule}) => rule.selectorList.text === '#main')
          ?.rule;

  testRunner.log(rule.style.cssProperties.filter(prop => prop.text));

  testRunner.completeTest();
});
