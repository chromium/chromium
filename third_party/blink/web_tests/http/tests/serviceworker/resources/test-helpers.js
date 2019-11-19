// Adapter for testharness.js-style tests with Service Workers

function service_worker_unregister_and_register(test, url, scope, options = {}) {
  if (!scope || scope.length == 0)
    return Promise.reject(new Error('tests must define a scope'));

  options.scope = scope;
  return service_worker_unregister(test, scope)
    .then(function() {
        return navigator.serviceWorker.register(url, options);
      })
    .catch(unreached_rejection(test,
                               'unregister and register should not fail'));
}

function service_worker_unregister(test, documentUrl) {
  return navigator.serviceWorker.getRegistration(documentUrl)
    .then(function(registration) {
        if (registration)
          return registration.unregister();
      })
    .catch(unreached_rejection(test, 'unregister should not fail'));
}

function service_worker_unregister_and_done(test, scope) {
  return service_worker_unregister(test, scope)
    .then(test.done.bind(test));
}

function unreached_fulfillment(test, prefix) {
  return test.step_func(function(result) {
      var error_prefix = prefix || 'unexpected fulfillment';
      assert_unreached(error_prefix + ': ' + result);
    });
}

// Rejection-specific helper that provides more details
function unreached_rejection(test, prefix) {
  return test.step_func(function(error) {
      var reason = error.message || error.name || error;
      var error_prefix = prefix || 'unexpected rejection';
      assert_unreached(error_prefix + ': ' + reason);
    });
}

// Adds an iframe to the document and returns a promise that resolves to the
// iframe when it finishes loading. When |options.auto_remove| is set to
// |false|, the caller is responsible for removing the iframe
// later. Otherwise, the frame will be removed after all tests are finished.
function with_iframe(url, options) {
  return new Promise(function(resolve) {
      var frame = document.createElement('iframe');
      frame.src = url;
      // Make sure the iframe stays in the viewport even if the test creates
      // many of them. Otherwise some iframes may end up being throttled.
      frame.style.position = 'absolute';
      frame.onload = function() { resolve(frame); };
      document.body.appendChild(frame);
      if (typeof options === 'undefined')
        options = {};
      if (typeof options.auto_remove === 'undefined')
        options.auto_remove = true;
      if (options.auto_remove)
        add_completion_callback(function() { frame.remove(); });
    });
}

function with_sandboxed_iframe(url, sandbox) {
  return new Promise(function(resolve) {
      var frame = document.createElement('iframe');
      frame.sandbox = sandbox;
      frame.src = url;
      frame.onload = function() { resolve(frame); };
      document.body.appendChild(frame);
    });
}

function normalizeURL(url) {
  return new URL(url, self.location).toString().replace(/#.*$/, '');
}

function wait_for_update(test, registration) {
  if (!registration || registration.unregister == undefined) {
    return Promise.reject(new Error(
      'wait_for_update must be passed a ServiceWorkerRegistration'));
  }

  return new Promise(test.step_func(function(resolve) {
      registration.addEventListener('updatefound', test.step_func(function() {
          resolve(registration.installing);
        }));
    }));
}

function wait_for_state(test, worker, state) {
  if (!worker || worker.state == undefined) {
    return Promise.reject(new Error(
      'wait_for_state must be passed a ServiceWorker'));
  }
  if (worker.state === state)
    return Promise.resolve(state);

  if (state === 'installing') {
    switch (worker.state) {
      case 'installed':
      case 'activating':
      case 'activated':
      case 'redundant':
        return Promise.reject(new Error(
          'worker is ' + worker.state + ' but waiting for ' + state));
    }
  }

  if (state === 'installed') {
    switch (worker.state) {
      case 'activating':
      case 'activated':
      case 'redundant':
        return Promise.reject(new Error(
          'worker is ' + worker.state + ' but waiting for ' + state));
    }
  }

  if (state === 'activating') {
    switch (worker.state) {
      case 'activated':
      case 'redundant':
        return Promise.reject(new Error(
          'worker is ' + worker.state + ' but waiting for ' + state));
    }
  }

  if (state === 'activated') {
    switch (worker.state) {
      case 'redundant':
        return Promise.reject(new Error(
          'worker is ' + worker.state + ' but waiting for ' + state));
    }
  }

  return new Promise(test.step_func(function(resolve) {
      worker.addEventListener('statechange', test.step_func(function() {
          if (worker.state === state)
            resolve(state);
        }));
    }));
}

// Declare a test that runs entirely in the ServiceWorkerGlobalScope. The |url|
// is the service worker script URL. This function:
// - Instantiates a new test with the description specified in |description|.
//   The test will succeed if the specified service worker can be successfully
//   registered and installed.
// - Creates a new ServiceWorker registration with a scope unique to the current
//   document URL and the script URL. This allows more than one
//   service_worker_test() to be run from the same document.
// - Waits for the new worker to begin installing.
// - Imports tests results from tests running inside the ServiceWorker.
function service_worker_test(url, description) {
  // If the document URL is https://example.com/document and the script URL is
  // https://example.com/script/worker.js, then the scope would be
  // https://example.com/script/scope/document/script/worker.js.
  var document_path = window.location.pathname;
  var script_path = new URL(url, window.location).pathname;
  var scope = new URL('scope' + document_path + script_path,
                      new URL(url, window.location)).toString();
  promise_test(function(test) {
      return service_worker_unregister_and_register(test, url, scope)
        .then(function(registration) {
            add_completion_callback(function() {
                registration.unregister();
              });
            return wait_for_update(test, registration)
              .then(function(worker) {
                  return fetch_tests_from_worker(worker);
                });
          });
    }, description);
}

function base_path() {
  return location.pathname.replace(/\/[^\/]*$/, '/');
}

function test_login(test, origin, username, password, cookie, cookie_cross_site) {
  return new Promise(function(resolve, reject) {
      with_iframe(
        origin +
        '/serviceworker/resources/fetch-access-control-login.html')
        .then(test.step_func(function(frame) {
            var channel = new MessageChannel();
            channel.port1.onmessage = test.step_func(function() {
                frame.remove();
                resolve();
              });
            frame.contentWindow.postMessage(
              {username: username, password: password, cookie: cookie, cookieCrossSite: cookie_cross_site},
              origin, [channel.port2]);
          }));
    });
}

function login(test, local, remote) {
  var suffix = (local.indexOf("https") != -1) ? "s": "";
  return test_login(test, local, 'username1' + suffix, 'password1' + suffix,
                    'cookie1', false /* cookie_cross_site */)
    .then(function() {
        return test_login(test, remote, 'username2' + suffix,
                          'password2' + suffix, 'cookie2', true /* cookie_cross_site */);
      });
}

function wait_for_port_message(port, handler) {
  return new Promise((resolve, reject) => {
    port.onmessage = (e) => {
      try {
        resolve(handler(e));
      } catch (e) {
        reject(e);
      }
    };
  });
}
