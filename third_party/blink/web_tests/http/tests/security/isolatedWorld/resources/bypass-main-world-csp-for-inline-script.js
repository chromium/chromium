if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

tests = 6;
mainWorld = true;
window.addEventListener("message", function(message) {
    tests -= 1;
    test();
}, false);

function test() {
    function injectInlineScript(isolated) {
        var script = document.createElement('script');
        var isolatedStr = isolated ? 'isolated world' : 'main world';
        script.innerText = `console.log('EXECUTED in ${isolatedStr}.');`;
        document.body.appendChild(script);
    }
    function injectInlineEventHandler(isolated) {
      // Inline event handlers are evaluated in the main world. See
      // crbug.com/912069.
      var div = document.createElement('div');
      div.innerHTML = '<div onclick=\'console.log(`click`)\'></div>';
      document.body.appendChild(div);
      div.firstChild.click();
    }

    function injectInlineScriptUsingDocumentWrite(isolated) {
      // Note the the behavior of document.write is quite unusual currently.
      // See crrev.com/c/chromium/src/+/2236957/4/third_party/blink/web_tests/http/tests/security/isolatedWorld/resources/bypass-main-world-csp-for-inline-script.js#33
      // for more details.
      var isolatedStr = isolated ? 'isolated world' : 'main world';
      var iframe = document.createElement('iframe');
      document.body.appendChild(iframe);
      var iframeDocument = iframe.contentWindow.document;
      iframeDocument.open();
      iframeDocument.write(`<script>console.log('Executed using document.write in ${isolatedStr}. Is main world: ' + parent.mainWorld);</script>`);
      iframeDocument.close();
    }

    function testInlineScript(isolated, worldId) {
      if (!isolated) {
        injectInlineScript(false);
        injectInlineEventHandler(false);
        injectInlineScriptUsingDocumentWrite(false);
        window.postMessage("next", "*");
        return;
      }

      testRunner.evaluateScriptInIsolatedWorld(
          worldId,
          String(eval('injectInlineScript')) + '\ninjectInlineScript(true);');
      testRunner.evaluateScriptInIsolatedWorld(
          worldId,
          String(eval('injectInlineEventHandler')) +
              '\ninjectInlineEventHandler(true);');
      testRunner.evaluateScriptInIsolatedWorld(
          worldId,
          String(eval('injectInlineScriptUsingDocumentWrite')) +
              '\ninjectInlineScriptUsingDocumentWrite(true);');
      testRunner.evaluateScriptInIsolatedWorld(worldId, 'window.postMessage("next", "*");');
    }

    switch (tests) {
      case 6:
        console.log('Injecting in main world: this should fail.');
        testInlineScript(false);
        break;
      case 5:
        console.log(
            "Injecting into isolated world without bypass: this should fail.");
        // This is needed because isolated worlds are not reset between test
        // runs and a previous test's CSP may interfere with this test. See
        // https://crbug.com/415845.
        testRunner.setIsolatedWorldInfo(1, null, null);

        testInlineScript(true, 1);
        break;
      case 4:
        console.log(
            'Allowing unsafe-inline for the isolated world: this should pass!');
        testRunner.setIsolatedWorldInfo(
            1, 'chrome-extension://123', 'script-src \'unsafe-inline\'');
        testInlineScript(true, 1);
        break;
      case 3:
        console.log('Disallowing unsafe-inline for the isolated world.');
        testRunner.setIsolatedWorldInfo(
            1, 'chrome-extension://123', 'script-src \'none\'');
        testInlineScript(true, 1);
        break;
      case 2:
        console.log(
            'Using an empty CSP for the isolated world. This should pass.');
        testRunner.setIsolatedWorldInfo(1, 'chrome-extension://123', '');
        testInlineScript(true, 1);
        break;
      case 1:
        console.log("Injecting into main world again: this should fail.");
        testInlineScript(false, 1);
        break;
      case 0:
        testRunner.setIsolatedWorldInfo(1, null, null);
        testRunner.notifyDone();
        break;
    }
}

document.addEventListener('DOMContentLoaded', test);
