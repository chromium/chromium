if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

window.addEventListener('message', function(message) {
  testCounter++;
  test();
}, false);

let testCounter = 1;

// This is needed because isolated worlds are not reset between test runs and a
// previous test's CSP may interfere with this test. See
// https://crbug.com/415845.
testRunner.setIsolatedWorldInfo(1, null, null);

function setFontFace(num) {
  let style = document.createElement('style');
  style.innerText =
      `@font-face { font-family: 'remote'; src: url(/resources/Ahem.ttf?num=${
          num}); }`;
  document.getElementById('body').appendChild(style);
  window.postMessage('next', '*');
}

function test() {
  switch (testCounter) {
    case 1:
      alert('With lax isolated world CSP');
      testRunner.setIsolatedWorldInfo(
          1, 'chrome-extension://123', 'font-src *');
      testRunner.evaluateScriptInIsolatedWorld(
          1, String(eval('setFontFace')) + '\nsetFontFace(1);');
      break;
    case 2:
      alert('With strict isolated world CSP');
      testRunner.setIsolatedWorldInfo(
          1, 'chrome-extension://123', 'font-src \'none\'');
      testRunner.evaluateScriptInIsolatedWorld(
          1, String(eval('setFontFace')) + '\nsetFontFace(2);');
      break;
    case 3:
      testRunner.notifyDone();
  }
}

document.addEventListener('DOMContentLoaded', test);
