// META: script=/common/get-host-info.sub.js
// META: script=resources/dedicated-worker-test.js
//
// The following tests assume the policy `Connection-Allowlist:
// (response-origin)` has been set.

// 1. Same-origin worker subresource fetch.
// origin: local scheme (allowlist inherited from creator document)
// This should SUCCEED because Connection-Allowlist is not enabled via
// Origin-Trial header.
worker_fetch_test(
    get_host_info().HTTPS_ORIGIN, SUCCESS,
    'Same-origin worker subresource fetch succeeds without origin trial header');

// 2. Cross-origin worker subresource fetch.
// origin: local scheme (allowlist inherited from creator document)
// This should SUCCEED because Connection-Allowlist is not enabled via
// Origin-Trial header.
worker_fetch_test(
    get_host_info().HTTPS_REMOTE_ORIGIN, SUCCESS,
    'Cross-origin worker subresource fetch succeeds without origin trial header');

// 3. Same-origin worker script fetch.
// origin: http://{{hosts[][]}} (allowed by allowlist)
// This should SUCCEED because Connection-Allowlist is not enabled via
// Origin-Trial header.
worker_script_fetch_test(
    get_host_info().HTTPS_ORIGIN, SUCCESS,
    'Same-origin worker script fetch succeeds without origin trial header');

// 4. Cross-origin worker script fetch.
// origin: http://{{hosts[][www1]}} (allowed by allowlist)
// This should FAIL because dedicated workers cannot be constructed with cross-
// origin scripts:
// https://developer.mozilla.org/en-US/docs/Web/API/Worker/Worker#url
worker_script_fetch_test(
    get_host_info().HTTPS_REMOTE_ORIGIN, FAILURE,
    'Cross-origin worker script fetch fails without origin trial header');
