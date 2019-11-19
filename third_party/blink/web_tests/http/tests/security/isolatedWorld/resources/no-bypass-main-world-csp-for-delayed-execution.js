if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

tests = 3;
window.addEventListener("message", function(event) {
    if (event.data == "next")
        tests -= 1;
    test(event.data);
}, false);

function test(message) {
    function injectInlineScript(script) {
        var scriptTag = document.createElement('script');
        scriptTag.innerText = script;
        document.body.appendChild(scriptTag);
        window.postMessage("next", "*");
    }

    function injectButtonWithInlineClickHandler(script) {
        var divTag = document.createElement('div');
        divTag.innerHTML = "<button id='button' onclick='" + script + "'></button>";
        document.body.appendChild(divTag);
        window.postMessage("done", "*");
    }

    var permissiveCSP = "script-src * 'unsafe-eval' 'unsafe-inline'";
    var securityOrigin = "chrome-extension://123";

    switch (tests) {
        case 3:
            testRunner.setIsolatedWorldInfo(1, securityOrigin, permissiveCSP);
            testRunner.evaluateScriptInIsolatedWorld(1, String(injectInlineScript) + "\ninjectInlineScript('try { alert(\"PASS: Case " + tests + " was not blocked by a CSP.\"); } catch (e) { alert(\"FAIL: Case " + tests + " should not be blocked by a CSP.\"); }');");
            break;
        case 2:
            testRunner.setIsolatedWorldInfo(1, securityOrigin, permissiveCSP);
            testRunner.evaluateScriptInIsolatedWorld(1, String(injectInlineScript) + "\ninjectInlineScript('try { eval(\"alert(\\\'FAIL: Case " + tests + " should have been blocked by a CSP.\\\');\"); } catch( e) { console.log(e); alert(\\\'PASS: Case " + tests + " was blocked by a CSP.\\\'); }');");
            break;
        case 1:
            if (message != "done") {
                testRunner.setIsolatedWorldInfo(1, securityOrigin, permissiveCSP);
                document.clickMessage = "PASS: Case " + tests + " was not evaluated in main world.";
                // The listener defined inline by injectButtonWithInlineClickHandler should be evaluated in the main world instead of an isolated world.
                testRunner.evaluateScriptInIsolatedWorld(1, String(injectButtonWithInlineClickHandler) + "\ninjectButtonWithInlineClickHandler('document.clickMessage =\"FAIL: Case " + tests + " was evaluated in isolated world.\"');");
            } else {
                document.getElementById("button").click();
                alert(document.clickMessage);
                alert(testRunner.evaluateScriptInIsolatedWorldAndReturnValue(1, "document.clickMessage"));
                window.postMessage("next", "*");
            }
            break;
        case 0:
            testRunner.setIsolatedWorldInfo(1, null, null);
            testRunner.notifyDone();
            break;
    }
}

document.addEventListener('DOMContentLoaded', test);
