// META: script=/common/get-host-info.sub.js
// META: script=resources/dedicated-worker-test.js
//
// The following tests assume the policy `Connection-Allowlist:
// (response-origin)` has been set.
// The response headers also contain the token present in
// resources/origin-trial-token.txt

// 1. Same-origin worker subresource fetch.
// origin: local scheme (allowlist inherited from creator document)
// This should SUCCEED because Connection-Allowlist is enabled with
// (response-origin)
worker_fetch_test(
    get_host_info().HTTPS_ORIGIN, SUCCESS,
    'Same-origin worker subresource fetch succeeds with origin trial header');

// 2. Cross-origin worker subresource fetch.
// origin: local scheme (allowlist inherited from creator document)
// This should FAIL because Connection-Allowlist is enabled with
// (response-origin), which is HTTPS_ORIGIN.
worker_fetch_test(
    get_host_info().HTTPS_REMOTE_ORIGIN, FAILURE,
    'Cross-origin worker subresource fetch fails with origin trial header');

// 3. Same-origin worker script fetch.
// origin: http://{{hosts[][]}} (allowed by allowlist)
// This should SUCCEED because Connection-Allowlist is enabled and the script
// fetch is same-origin.
worker_script_fetch_test(
    get_host_info().HTTPS_ORIGIN, SUCCESS,
    'Same-origin worker script fetch succeeds with origin trial header');

// 4. Cross-origin worker script fetch.
// origin: http://{{hosts[][www1]}} (allowed by allowlist)
// This should FAIL because dedicated workers cannot be constructed with cross-
// origin scripts:
// https://developer.mozilla.org/en-US/docs/Web/API/Worker/Worker#url
worker_script_fetch_test(
    get_host_info().HTTPS_REMOTE_ORIGIN, FAILURE,
    'Cross-origin worker script fetch fails with origin trial header');
