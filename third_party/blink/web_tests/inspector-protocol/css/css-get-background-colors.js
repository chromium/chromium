(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL('resources/css-get-background-colors.html', 'Test css.getBackgroundColors method');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  async function testGetBgColors(querySelector) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.getBackgroundColors({nodeId});
    testRunner.log(JSON.stringify(response.result));
  }

  testRunner.runTestSuite([
    async function testNoText() {
      testRunner.log('No text (and not a positioned element): should be null');
      await testGetBgColors('.noText p');
    },
    async function testNoBgColor() {
      testRunner.log('No background color: should be white');
      await testGetBgColors('.noBgColor p');
    },
    async function testOpaqueBgColor() {
      testRunner.log('Opaque background color: should be red');
      await testGetBgColors('.opaqueBgColor p');
    },
    async function testLayeredOpaqueBgColors() {
      testRunner.log('Opaque background color in front of another opaque background color: should be blue');
      await testGetBgColors('.layeredOpaqueBgColors p');
    },
    async function testOneSemitransparentBgColor() {
      testRunner.log('Semi-transparent background color: should be light red');
      await testGetBgColors('.semitransparentBgColor p');
    },
    async function testTwoSemitransparentBgColors() {
      testRunner.log('Two layered semi-transparent background colors: should be medium red');
      await testGetBgColors('.twoSemitransparentBgColors p');
    },
    async function testOpaqueGradientBackground() {
      testRunner.log('Opaque gradient: should be red and black');
      await testGetBgColors('.opaqueGradientBg p');
    },
    async function testOpaqueGradientBackgroundBehindScrim() {
      testRunner.log('Opaque gradient behind semi-transparent color: should be light red and 50% grey');
      await testGetBgColors('.opaqueGradientBehindScrim p');
    },
    async function testOpaqueGradientBackgroundWithColorBackground() {
      testRunner.log('Opaque gradient and solid color background on same element: should be red and black');
      await testGetBgColors('.opaqueGradientAndColorBg p');
    },
    async function testPartiallyTransparentGradientBackground() {
      testRunner.log('Semi-transparent gradient: should be light red and 50% grey');
      await testGetBgColors('.semitransparentGradientBg p');
    },
    async function testPartiallyTransparentGradientAndColorBackground() {
      testRunner.log('Semi-transparent gradient and solid color on same element: should be dark red and 50% grey');
      await testGetBgColors('.semitransparentGradientAndColorBg p');
    },
    async function testTwoPartiallyTransparentGradientBackgrounds() {
      testRunner.log('Layered semi-transparent gradients: should be empty array');
      await testGetBgColors('.twoSemitransparentGradientBgs p');
    },
    async function testPartiallyOverlappingBackground() {
      testRunner.log('Partially overlapping background: should be empty array');
      await testGetBgColors('.partiallyOverlappingBackground p');
    },
    async function smallerBackground() {
      testRunner.log('Background smaller than text: should be empty array');
      await testGetBgColors('.smallerBackground p');
    },
    async function testObscuredPartiallyOverlappingBackground() {
      testRunner.log('Red background obscuring partially overlapping blue background: should be red only');
      await testGetBgColors('.obscuredPartiallyOverlappingBackground p');
    },
    async function testBackgroundImage() {
      testRunner.log('Background image: should be empty array');
      await testGetBgColors('.backgroundImage p');
    },
    async function testBackgroundImageAndBgColor() {
      testRunner.log('Background image with background color: should be empty array');
      await testGetBgColors('.backgroundImageAndBgColor p');
    },

    async function testBackgroundImageBehindScrim() {
      testRunner.log('Background image behind scrim: should be semi-transparent white');
      await testGetBgColors('.backgroundImageBehindScrim p');
    },
    async function testForegroundImage() {
      testRunner.log('Image behind text: should be empty array');
      await testGetBgColors('.foregroundImage p');
    },
    async function testForegroundImageBehindScrim() {
      testRunner.log('Image behind scrim: should be semi-transparent white');
      await testGetBgColors('.foregroundImageBehindScrim p');
    },
    async function testForegroundCanvas() {
      testRunner.log('Canvas behind text: should be empty array');
      await testGetBgColors('.canvas p');
    },
    async function testForegroundEmbed() {
      testRunner.log('Embed behind text: should be empty array');
      await testGetBgColors('.embed p');
    },
    async function testForegroundObject() {
      testRunner.log('Object behind text: should be empty array');
      await testGetBgColors('.object p');
    },
    async function testForegroundPicture() {
      testRunner.log('Picture behind text: should be empty array');
      await testGetBgColors('.picture p');
    },
    async function testForegroundSVG() {
      testRunner.log('SVG behind text: should be empty array');
      await testGetBgColors('.svg p');
    },
    async function testForegroundVideo() {
      testRunner.log('Video behind text: should be empty array');
      await testGetBgColors('.video p');
    },
    async function testShadowDOM() {
      testRunner.log('Background color in Shadow DOM: should be blue');
      await testGetBgColors('.shadowDOM > .shadowHost p');
    },
  ]);
});
