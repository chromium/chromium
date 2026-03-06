/* https://crbug.com/490023239 */
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
<!DOCTYPE html>
<style>
  .outer {
    @scope (.a) {
      .inner { color: red; }
    }
  }
</style>
<div class="outer"><div class="inner" id="t">x</div></div>`, 'Tests CSS.setScopeText with scope nested in a rule.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  const event = await dp.CSS.onceStyleSheetAdded();
  const styleSheetId = event.params.header.styleSheetId;
  const setScopeText = cssHelper.setScopeText.bind(cssHelper, styleSheetId, false);

  testRunner.runTestSuite([
    async function testNestedEdit() {
      await setScopeText({
        range: {
          startLine: 2,
          startColumn: 11,
          endLine: 2,
          endColumn: 15,
        },
        text: '(.b)',
      });

      await session.evaluateAsync(`
(() => {
  const sheet = document.styleSheets[0];
  const outer = sheet.cssRules[0];
  const scope = outer.cssRules[0];

  scope.deleteRule(0);
  scope.insertRule('@media all {}', 0);
  const media = scope.cssRules[0];

  outer.selectorText = '.outer2';
  media.insertRule('.boom {}', 0);

  getComputedStyle(document.getElementById('t')).color;
  document.body.offsetTop;
})()
`);
    },
  ]);
})
