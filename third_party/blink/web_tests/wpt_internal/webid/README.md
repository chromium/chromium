The `webid-helpers.js` exposes `webid_test` which is a specialized
`promise_test` which comes pre-setup with the appropriate mocking infrastructure
to emulate platform federated auth backend. The mock is passed to the test
function as the second parameter.

Example usage:
```
<script type="module">
  import {webid_test} from './resources/webid-helper.js';

  webid_test(async (t, mock) => {
    mock.returnIdToken("a_token");
    assert_equals("a_token", await navigator.id.get(options));
  }, "Successfully obtaining a token using mock.");
</script>
```

The mock interface, `MockFederatedAuthRequest`, is defined as:

```
class MockFederatedAuthRequest {
  // Causes the subsequent `navigator.id.get()` to resolve with the token.
  returnIdToken(string token);
  // Causes the subsequent `navigator.id.get()` to reject with the error.
  returnError(string error);
};
```

The chromium implementation uses the MojoJS shim.
