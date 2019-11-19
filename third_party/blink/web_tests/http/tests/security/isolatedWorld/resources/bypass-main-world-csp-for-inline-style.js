if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

tests = 6;
window.addEventListener("message", function(message) {
    tests -= 1;
    test();
}, false);

function test() {
    function injectInlineStyle(shouldSucceed, tests) {
        var id = 'inline' + tests;
        var div = document.createElement('div');
        div.id = id;
        document.body.appendChild(div);
        var style = document.createElement('style');
        style.innerText = '#' + id + ' { color: red; }';
        document.body.appendChild(style);
        var success = window.getComputedStyle(document.getElementById(id)).color === "rgb(255, 0, 0)";
        if (shouldSucceed) {
            if (success)
                console.log("PASS: Style assignment in test " + tests + " was not blocked by CSP.");
            else
                console.log("FAIL: Style assignment in test " + tests + " was blocked by CSP.");
        } else {
            if (success)
                console.log("FAIL: Style assignment in test " + tests + " was not blocked by CSP.");
            else
                console.log("PASS: Style assignment in test " + tests + " was blocked by CSP.");
        }
        window.postMessage("next", "*");
    }
    function injectInlineStyleAttribute(shouldSucceed, tests) {
        var id = 'attribute' + tests;
        var div = document.createElement('div');
        div.id = id;
        document.body.appendChild(div);
        div.setAttribute('style', 'color: red;');
        var success = window.getComputedStyle(document.getElementById(id)).color === "rgb(255, 0, 0)";
        if (shouldSucceed) {
            if (success)
                console.log("PASS: Style attribute assignment in test " + tests + " was not blocked by CSP.");
            else
                console.log("FAIL: Style attribute assignment in test " + tests + " was blocked by CSP.");
        } else {
            if (success)
                console.log("FAIL: Style attribute assignment in test " + tests + " was not blocked by CSP.");
            else
                console.log("PASS: Style attribute assignment in test " + tests + " was blocked by CSP.");
        }
        window.postMessage("next", "*");
    }

    function testInlineStyleInIsolatedWorldHelper(
        worldId, functionStr, shouldSucceedStr, tests) {
      testRunner.evaluateScriptInIsolatedWorld(
          worldId,
          String(eval(functionStr)) +
              `\n${functionStr}(${shouldSucceedStr}, ${tests});`);
    }

    function testInlineStyleInIsolatedWorld(worldId, shouldSucceed, tests) {
      var success = shouldSucceed ? 'true' : 'false';
      testInlineStyleInIsolatedWorldHelper(
          worldId, 'injectInlineStyle', success, tests);
      testInlineStyleInIsolatedWorldHelper(
          worldId, 'injectInlineStyleAttribute', success, tests);
    }

    switch (tests) {
      case 6:
        console.log("Injecting in main world: this should fail.");
        injectInlineStyle(false, tests);
        break;
      case 5:
        console.log(
            "Injecting into isolated world without bypass: this should fail.");
        // Clear any existing csp or security origin as a side effect of
        // another test.
        testRunner.setIsolatedWorldInfo(1, null, null);
        testInlineStyleInIsolatedWorld(1, false, tests);
        break;
      case 4:
        console.log(
            'Have a separate CSP for the isolated world. Allow unsafe-inline. This should pass.');
        testRunner.setIsolatedWorldInfo(
            1, 'chrome-extension://123', 'style-src \'unsafe-inline\'');
        testInlineStyleInIsolatedWorld(1, true, tests);
        break;
      case 3:
        console.log(
            'Have a separate CSP for the isolated world. Use an empty CSP. This should pass.');
        testRunner.setIsolatedWorldInfo(1, 'chrome-extension://123', '');
        testInlineStyleInIsolatedWorld(1, true, tests);
        break;
      case 2:
        console.log(
            'Have a separate CSP for the isolated world. Disallow unsafe-inline.');
        testRunner.setIsolatedWorldInfo(
            1, 'chrome-extension://123', 'style-src \'none\'');
        console.log(
            'internals.runtimeFlags.isolatedWorldCSPEnabled is ' +
            internals.runtimeFlags.isolatedWorldCSPEnabled);
        var allowUnsafeInline = !internals.runtimeFlags.isolatedWorldCSPEnabled;
        testInlineStyleInIsolatedWorld(1, allowUnsafeInline, tests);
        break;
      case 1:
        console.log("Injecting into main world again: this should fail.");
        injectInlineStyle(false, tests);
        break;
      case 0:
        testRunner.setIsolatedWorldInfo(1, null, null);
        testRunner.notifyDone();
        break;
    }
}

document.addEventListener('DOMContentLoaded', test);
