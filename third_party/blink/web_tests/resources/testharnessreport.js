/*
 * This file is intended for vendors to implement code needed to integrate
 * testharness.js tests with their own test systems.
 *
 * Typically such integration will attach callbacks when each test is
 * has run, using add_result_callback(callback(test)), or when the whole test
 * file has completed, using add_completion_callback(callback(tests,
 * harness_status)).
 *
 * For more documentation about the callback functions and the
 * parameters they are called with, see testharness.js, or the docs at:
 * https://web-platform-tests.org/writing-tests/testharness-api.html
 */

(function() {

    let outputDocument = document;
    let didDispatchLoadEvent = false;
    let localPathRegExp = undefined;

    // Setup for Blink JavaScript tests. self.testRunner is expected to be
    // present when tests are run.
    if (self.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
        testRunner.setCanOpenWindows();
        testRunner.setCloseRemainingWindowsWhenComplete(true);
        testRunner.setDumpJavaScriptDialogs(false);

        // Some tests intentionally load mixed content in order to test the
        // referrer policy, so WebKitAllowRunningInsecureContent must be set
        // to true or else the load would be blocked.
        const paths = [
            'service-workers/service-worker/fetch-event-referrer-policy.https.html',
            'beacon/headers/header-referrer-no-referrer-when-downgrade.https.html',
            'beacon/headers/header-referrer-strict-origin-when-cross-origin.https.html',
            'beacon/headers/header-referrer-strict-origin.https.html',
            'beacon/headers/header-referrer-unsafe-url.https.html',
        ];
        for (const path of paths) {
          if (document.URL.endsWith(path)) {
            testRunner.overridePreference('WebKitAllowRunningInsecureContent', true);
            break;
          }
        }
    }

    if (document.URL.startsWith('file:///')) {
        const index = document.URL.indexOf('/external/wpt');
        if (index >= 0) {
            const localPath = document.URL.substring(
                'file:///'.length, index + '/external/wpt'.length);
            localPathRegExp = new RegExp(localPath.replace(/(\W)/g, '\\$1'), 'g');
        }
    }

    window.addEventListener('load', loadCallback, {'once': true});

    setup({
        // The default output formats test results into an HTML table, but for
        // the Blink layout test runner, we dump the results as text in the
        // completion callback, so we disable the default output.
        'output': false,
        // The Blink layout test runner has its own timeout mechanism.
        'explicit_timeout': true
    });

    add_start_callback(startCallback);
    add_completion_callback(completionCallback);

    /** Loads an automation script if necessary. */
    function loadCallback() {
        didDispatchLoadEvent = true;
        if (isWPTManualTest()) {
            setTimeout(loadAutomationScript, 0);
        }
    }

    /** Checks whether the current path is a manual test in WPT. */
    function isWPTManualTest() {
        // Here we assume that if wptserve is running, then the hostname
        // is web-platform.test.
        const path = location.pathname;
        if (location.hostname == 'web-platform.test' &&
            /.*-manual(\.sub)?(\.https)?(\.tentative)?\.html$/.test(path)) {
            return true;
        }
        // If the file is loaded locally via file://, it must include
        // the wpt directory in the path.
        return /\/external\/wpt\/.*-manual(\.sub)?(\.https)?(\.tentative)?\.html$/.test(path);
    }

    /** Loads the WPT automation script for the current test, if applicable. */
    function loadAutomationScript() {
        const pathAndBase = pathAndBaseNameInWPT();
        if (!pathAndBase) {
            return;
        }
        let automationPath = location.pathname.replace(
            /\/external\/wpt\/.*$/, '/external/wpt_automation');
        if (location.hostname == 'web-platform.test') {
            automationPath = '/wpt_automation';
        }

        // Export importAutomationScript for use by the automation scripts.
        window.importAutomationScript = function(relativePath) {
            const script = document.createElement('script');
            script.src = automationPath + relativePath;
            document.head.appendChild(script);
        };

        let src;
        if (pathAndBase.startsWith('/fullscreen/')) {
            // Fullscreen tests all use the same automation script.
            src = automationPath + '/fullscreen/auto-click.js';
        } else if (
            pathAndBase.startsWith('/css/') ||
            pathAndBase.startsWith('/pointerevents/') ||
            pathAndBase.startsWith('/uievents/') ||
            pathAndBase.startsWith('/pointerlock/') ||
            pathAndBase.startsWith('/html/') ||
            pathAndBase.startsWith('/input-events/') ||
            pathAndBase.startsWith('/css/selectors/') ||
            pathAndBase.startsWith('/css/cssom-view/') ||
            pathAndBase.startsWith('/css/css-scroll-snap/') ||
            pathAndBase.startsWith('/dom/events/') ||
            pathAndBase.startsWith('/feature-policy/experimental-features/')) {
            // Per-test automation scripts.
            src = automationPath + pathAndBase + '-automation.js';
        } else {
            return;
        }
        const script = document.createElement('script');
        script.src = src;
        document.head.appendChild(script);
    }

    /**
     * Sets the output document based on the given properties.
     * Usually the output document is the current document, but it could be
     * a separate document in some cases.
     */
    function startCallback(properties) {
        if (properties.output_document) {
            outputDocument = properties.output_document;
        }
    }

    /**
     * Adds results to the page in a manner that allows dumpAsText to produce
     * readable test results.
     */
    function completionCallback(tests, harness_status) {
        const xhtmlNS = 'http://www.w3.org/1999/xhtml';

        // Create element to hold results.
        const resultsElement = outputDocument.createElementNS(xhtmlNS, 'pre');
        resultsElement.style.whiteSpace = 'pre-wrap';
        resultsElement.style.lineHeight = '1.5';

        // Declare result string.
        let resultStr = 'This is a testharness.js-based test.\n';

        // Check harness_status.  If it is not 0, tests did not execute
        // correctly, output the error code and message.
        if (harness_status.status != 0) {
            resultStr += `Harness Error. ` +
                `harness_status.status = ${harness_status.status} , ` +
                `harness_status.message = ${harness_status.message}\n`;
        }

        // Output failure metrics if there are many.
        resultCounts = countResultTypes(tests);
        if (outputDocument.URL.indexOf('://web-platform.test') >= 0 &&
            tests.length >= 50 &&
            (resultCounts[1] || resultCounts[2] || resultCounts[3])) {

            resultStr += failureMetricSummary(resultCounts);
        }

        if (outputDocument.URL.indexOf('/html/dom/reflection') >= 0) {
            resultStr += compactTestOutput(tests);
        } else {
            resultStr += testOutput(tests);
        }

        resultStr += 'Harness: the test ran to completion.\n';

        resultsElement.textContent = resultStr;

        function done() {
            let body = null;
            if (outputDocument.body && outputDocument.body.tagName == 'BODY' &&
                outputDocument.body.namespaceURI == xhtmlNS) {
                body = outputDocument.body;
            }
            // A temporary workaround since |window.self| property lookup starts
            // failing if the frame is detached. |outputDocument| may be an
            // ancestor of |self| so clearing |textContent| may detach |self|.
            // To get around this, cache window.self now and use the cached
            // value.
            // TODO(dcheng): Remove this hack after fixing window/self/frames
            // lookup in https://crbug.com/618672
            const cachedSelf = window.self;
            if (cachedSelf.testRunner) {
                // The following DOM operations may show console messages.  We
                // suppress them because they are not related to the running
                // test.
                testRunner.setDumpConsoleMessages(false);

                // Anything in the body isn't part of the output and so should
                // be hidden from the text dump.
                if (body) {
                    body.textContent = '';
                }
            }

            // Add the results element to the output document.
            if (!body) {
                // |outputDocument| might be an SVG document.
                if (outputDocument.documentElement) {
                    outputDocument.documentElement.remove();
                }
                let html = outputDocument.createElementNS(xhtmlNS, 'html');
                outputDocument.appendChild(html);
                body = outputDocument.createElementNS(xhtmlNS, 'body');
                body.setAttribute('style', 'white-space:pre;');
                html.appendChild(body);
            }
            outputDocument.body.appendChild(resultsElement);

            // IFrames running tests should not complete the harness as the parent
            // page will.
            let shouldCompleteHarness = (window.self == window.top);
            if (cachedSelf.testRunner && shouldCompleteHarness) {
                testRunner.notifyDone();
            }
        }

        if (didDispatchLoadEvent || outputDocument.readyState != 'loading') {
            // This function might not be the last completion callback, and
            // another completion callback might generate more results.
            // So, we don't dump the results immediately.
            setTimeout(done, 0);
        } else {
            // Parsing the test HTML isn't finished yet.
            window.addEventListener('load', done);
        }
    }

    /**
     * Returns a directory part relative to WPT root and a basename part of the
     * current test. e.g.
     * Current test: file:///.../LayoutTests/external/wpt/pointerevents/foobar.html
     * Output: "/pointerevents/foobar"
     */
    function pathAndBaseNameInWPT() {
        const path = location.pathname;
        let matches;
        if (location.hostname == 'web-platform.test') {
            matches = path.match(/^(\/.*)\.html$/);
            return matches ? matches[1] : null;
        }
        matches = path.match(/external\/wpt(\/.*)\.html$/);
        return matches ? matches[1] : null;
    }

    /** Converts the testharness test status into the corresponding string. */
    function convertResult(resultStatus) {
        switch (resultStatus) {
            case 0:
                return 'PASS';
            case 1:
                return 'FAIL';
            case 2:
                return 'TIMEOUT';
            default:
                return 'NOTRUN';
        }
    }

    /**
     * Returns a compact output for reflection test results.
     *
     * The reflection tests contain a large number of tests.
     * This test output merges PASS lines to make baselines smaller.
     */
    function compactTestOutput(tests) {
        let testResults = [];
        for (let i = 0; i < tests.length; ++i) {
            if (tests[i].status == 0) {
                const colon = tests[i].name.indexOf(':');
                if (colon > 0) {
                    const prefix = tests[i].name.substring(0, colon + 1);
                    let j = i + 1;
                    for (; j < tests.length; ++j) {
                        if (!tests[j].name.startsWith(prefix) ||
                            tests[j].status != 0)
                            break;
                    }
                    const numPasses = j - i;
                    if (numPasses > 1) {
                        testResults.push(
                            `${convertResult(tests[i].status)} ` +
                            `${sanitize(prefix)} ${numPasses} tests\n`);
                        i = j - 1;
                        continue;
                    }
                }
            }
            testResults.push(resultLine(tests[i]));
        }
        return testResults.join('');
    }

    function testOutput(tests) {
        let testResults = '';
        window.tests = tests;
        for (let test of tests) {
            testResults += resultLine(test);
        }
        return testResults;
    }

    function resultLine(test) {
        let result = `${convertResult(test.status)} ${sanitize(test.name)}`;
        if (test.message) {
            result += ' ' + sanitize(test.message).trim();
        }
        return result + '\n';
    }

    /** Prepares the given text for display in test results. */
    function sanitize(text) {
        if (!text) {
            return '';
        }
        // Escape null characters, otherwise diff will think the file is binary.
        text = text.replace(/\0/g, '\\0');
        // Escape some special characters to improve readability of the output.
        text = text.replace(/\r/g, '\\r');

        // Replace machine-dependent path with "...".
        if (localPathRegExp) {
            text = text.replace(localPathRegExp, '...');
        }
        return text;
    }

    function countResultTypes(tests) {
        const resultCounts = [0, 0, 0, 0];
        for (let test of tests) {
            resultCounts[test.status]++;
        }
        return resultCounts;
    }

    function failureMetricSummary(resultCounts) {
        const total = resultCounts[0] + resultCounts[1] + resultCounts[2] + resultCounts[3];
        return `Found ${total} tests;` +
            ` ${resultCounts[0]} PASS,` +
            ` ${resultCounts[1]} FAIL,` +
            ` ${resultCounts[2]} TIMEOUT,` +
            ` ${resultCounts[3]} NOTRUN.\n`;
    }

})();
