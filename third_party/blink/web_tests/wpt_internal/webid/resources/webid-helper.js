// The testing infra for WebID is loaded using mojo js shim. To enable these
// tests the browser must be run with these options:
//
//   --enable-blink-features=MojoJS,MojoJSTest

import { MockFederatedAuthRequest } from './webid-mock.js';

export function webid_test(test_func, name, properties) {
  promise_test(async (t) => {
    assert_implements(navigator.id, 'missing navigator.id');
    const mock = new MockFederatedAuthRequest();
    try {
      await test_func(t, mock);
    } finally {
      await mock.reset();
    }
  }, name, properties);
}
