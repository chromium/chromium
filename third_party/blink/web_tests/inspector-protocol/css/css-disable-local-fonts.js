(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
<link rel="preload" href="${testRunner.url('./resources/Ahem.ttf')}" as="font">
<style>
  @font-face {
    font-family: 'LocalSansWithAhemWebFallback';
    src:
      local('Arial'), /* win/mac */
      local('DejaVu Sans'), /* linux */
      url('${testRunner.url('./resources/Ahem.ttf')}');
  }
  p {
    font-family: 'LocalSansWithAhemWebFallback';
  }
</style>
<p>test text</p>
`, 'Test css.setLocalFontsEnabled method.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.runTestSuite([
    async function testElementFontsWithLocalFontsEnabled(next) {
      testRunner.log('Expected p font: "Arial local" on Win/Mac and "DejaVu Sans" on Linux');
      await platformFontsForElementWithSelector(documentNodeId, 'p');
    },
    async function testElementFontsWithLocalFontsDisabled(next) {
      testRunner.log('Expected p font: "Ahem network"');
      await dp.CSS.setLocalFontsEnabled({ enabled: false });
      // Reset styles to force new fonts
      await session.evaluateAsync(() => {
        const content = document.getElementsByTagName('style')[0].textContent;
        document.getElementsByTagName('style')[0].textContent = '';
        document.getElementsByTagName('style')[0].textContent = content;
        return new Promise(resolve => requestAnimationFrame(resolve));
      });
      await platformFontsForElementWithSelector(documentNodeId, 'p');
    }
  ]);

  async function platformFontsForElementWithSelector(documentNodeId, selector) {
    const nodeId = await cssHelper.requestNodeId(documentNodeId, selector);
    const response = await dp.CSS.getPlatformFontsForNode({ nodeId });
    for (const font of response.result.fonts) {
      testRunner.log(`Actual p font: "${font.familyName} ${font.isCustomFont ? 'network' : 'local'}"`);
    }
  }
});
