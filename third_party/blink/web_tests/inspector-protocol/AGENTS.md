# Inspector Protocol Tests (`third_party/blink/web_tests/inspector-protocol`)

This directory contains JavaScript Inspector Protocol tests for the Chrome DevTools Protocol (CDP).

## Test Directory Locations

- **Standard tests (no web server needed):**
  `third_party/blink/web_tests/inspector-protocol/<domain>/<test-name>.js`
- **Tests requiring an HTTP server (e.g., cookies, cross-origin navigations, network headers):**
  `third_party/blink/web_tests/http/tests/inspector-protocol/<domain>/<test-name>.js`

## Test Structure Example

Each test is written as an asynchronous IIFE receiving a `testRunner` instance:

```javascript
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Description of the test case.');

  // Enable necessary domains
  await dp.Page.enable();
  await dp.Runtime.enable();

  // Send protocol commands and log results
  const result = await dp.Runtime.evaluate({expression: 'window.location.href'});
  testRunner.log(result.result);

  // Mark the test complete
  testRunner.completeTest();
})
```

## Building, Running, and Updating Expectations

Inspector Protocol tests run as Blink web tests using `blink_tests` and `third_party/blink/tools/run_web_tests.py` (pass `--reset-results` when running to generate or update the companion `-expected.txt` baseline file).

For complete instructions on building and running web tests, see:
- **[Blink `renderer/core` Development Guidelines (`/third_party/blink/renderer/core/AGENTS.md`)](/third_party/blink/renderer/core/AGENTS.md)**

## Related Documentation
- [Canonical CDP Testing Guide (`/content/browser/devtools/protocol/AGENTS.md`)](/content/browser/devtools/protocol/AGENTS.md)
- [Blink `renderer/core` Development Guidelines (`/third_party/blink/renderer/core/AGENTS.md`)](/third_party/blink/renderer/core/AGENTS.md)
