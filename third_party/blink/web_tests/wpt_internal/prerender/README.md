# Prerender WPT tests

This directory contains [Web Platform
Tests](third_party/blink/web_tests/external/wpt) for [Prerendering
Revamped](https://wicg.github.io/nav-speculation/prerendering.html).

These tests are generally intended to be upstreamed to the Web Platform Tests
repository (i.e., moved from wpt_internal to external/wpt). There are various
reasons they cannot be upstreamed today. The main blocker is
https://crbug.com/1226460.

In general, these tests should follow Chromium's [web tests
guidelines](docs/testing/web_tests_tips.md) and [web-platform-tests
guidelines](/docs/testing/web_platform_tests.md). This document describes
additional conventions for these particular tests.

## Describe requirements for upstreaming the test

All tests should have a comment near the top of the file that explains what must
change before the test can be upstreamed. Typical reasons include:
* The need for a [WebDriver API](https://crbug.com/1226460) to get a permission.
  See the later section for details.
* The tests expects behaviors that are not yet described in the specification.

**Good:**
```html
<!--
This file cannot be upstreamed to WPT until:
* internals.setPermission() usage is replaced with a WebDriver API
* The specification describes the loading of cross-origin iframes. The test
  expects that they are not loaded during prerendering.
-->
```

## Use the long timeout

These tests tend to be slow. The probable reason for the slowness is that they
do several navigations, for the main test page, the "initial" page that triggers
prerendering, the prerendered page, and the activation.

All tests should use the "long" WPT test [harness
timeout](https://web-platform-tests.org/writing-tests/testharness-api.html#harness-timeout).

**Good:**
```html
<meta name="timeout" content="long">
```

## Use internals.setPermission() to test permission-based APIs

For tests that use permission-gated APIs, use `internals.setPermission()` to
grant permission to the API before using it.

**Good:**

```js
// Grant the permission here to make a more discerning test because
// navigator.wakeLock.request() waits until the permission is granted, which
// may be deferred during prerendering, so the test would trivially pass without
// the permission.
await internals.setPermission({name: 'screen-wake-lock'}, 'granted',
                              location.origin, location.origin);
```

## Propagate `assert_*` results up to the `promise_test`

It is often convenient to use `assert_equals` and other asserts outside of the
test itself. For example, the triggering page or prerendering page may want to
do asserts. However, if these asserts are just used without messaging the main
test page, the `promise_test` does not detect assertion failures.  Instead, the
test will only have stderr output from the console, and it may timeout instead
of cleanly failing.

To avoid this, enclose the `assert`s in `try/catch` blocks, and message the
error to the main test page, so that the `promise_test` can detect the failure.

**Good:**

In my-test.html (the test page):
```js
const channel = new BroadcastChannel('test-channel');

const gotMessage = new Promise(resolve => {
  channel.addEventListener('message', e => {
    resolve(e.data);
  }, {once: true});
});

const msg = await gotMessage;
assert_equals(msg, 'PASS');
```

In resources/my-test.html (the prerendered page):
```js
async function main() {
  assert_equals('a', 'b');
}

// Run main() then message the test page with the result.
const testChannel = new BroadcastChannel('test-channel');
main().then(() => {
  testChannel.postMessage('PASS');
}).catch((e) => {
  testChannel.postMessage('FAIL: ' + e);
});
```

This test fails cleanly with output like:
```
FAIL assert_equals: expected "PASS" but got "FAIL: Error: assert_equals:
expected 'b' but got 'a'"
```

**Bad:**

In resources/my-test.html (the prerendered page):

```js
assert_equals('a', 'b');
const testChannel = new BroadcastChannel('test-channel');
testChannel.postMessage('PASS');
```

This test might timeout instead of fail by assertion failure.

## Wrap lines at 80 columns

This is the convention for most Chromium/WPT style tests. Note that
`git cl format [--js]` does not reformat js code in .html files.
