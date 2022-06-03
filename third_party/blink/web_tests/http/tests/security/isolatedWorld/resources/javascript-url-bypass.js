if (window.testRunner) {
  testRunner.dumpAsText();
  testRunner.waitUntilDone();
}

let tests = 1;
window.addEventListener('message', function(message) {
  tests += 1;
  test();
}, false);

function setIframeSrcToJavaScript() {
  const iframe = document.createElement('iframe');
  document.body.appendChild(iframe);

  // Use a timeout to ensure the javascript alert below is executed before the
  // next test begins.
  setTimeout(() => {
    window.postMessage('next', '*');
  }, 100);

  iframe.src = 'javascript:alert(\'iframe javascript: src running\')';
}

const isolatedWorldID = 1;
function testJavaScriptUrlInIsolatedWorld() {
  testRunner.evaluateScriptInIsolatedWorld(
      isolatedWorldID,
      String(eval('setIframeSrcToJavaScript')) +
          '\nsetIframeSrcToJavaScript();');
}

function test() {
  alert('Running test #' + tests);
  switch (tests) {
    case 1:
      alert('Isolated world with no CSP');
      testRunner.setIsolatedWorldInfo(isolatedWorldID, null, null);
      testJavaScriptUrlInIsolatedWorld();
      break;
    case 2:
      alert('Isolated world with permissive CSP');
      testRunner.setIsolatedWorldInfo(
          isolatedWorldID, 'chrome-extension://123',
          'script-src \'unsafe-inline\'');
      testJavaScriptUrlInIsolatedWorld();
      break;
    case 3:
      alert('Isolated world with strict CSP');
      testRunner.setIsolatedWorldInfo(
          isolatedWorldID, 'chrome-extension://123', 'script-src \'none\'');
      testJavaScriptUrlInIsolatedWorld();
      break;
    case 4:
      testRunner.notifyDone();
      break;
  }
}

document.addEventListener('DOMContentLoaded', test);
