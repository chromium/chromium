(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'https://example.test:8443/inspector-protocol/resources/test-page.html',
      `Tests that willSendRequest contains the correct mixed content status for passive mixed content.`);

  // The iframe is in the same site so we can watch its network requests.
  function addIframeWithMixedContent() {
    var iframe = document.createElement('iframe');
    iframe.src = 'https://example.test:8443/inspector-protocol/resources/passive-mixed-content-iframe.html';
    document.body.appendChild(iframe);
  }

  var helper = await testRunner.loadScript('./resources/mixed-content-type-test.js');
  helper(testRunner, session, addIframeWithMixedContent);
})
