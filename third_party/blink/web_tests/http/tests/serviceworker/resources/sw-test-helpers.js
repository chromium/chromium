// Test helper that is meant as a mini test framework to be used from a service
// worker that runs some tests and send results back to its client.
//
// A simple usage of this framework would consist of calling initialize() to
// setup then runNextTestOrQuit() in order to start running the methods defined
// by TESTS. Then, tests can start sending messages back to the client using
// postMessage().
//
// Example:
// var TESTS = [
//     function simpleTest() {
//         self.postMessage('you will receive this first');
//     },
//     function secondTest() {
//         self.postMessage('secondTest done!');
//         runNextTestOrQuit();
//     }
// ];
//
// initialize().runNextTestOrQuit();
//
// In addition, there is a helper method meant to synthesized notificationclick
// events sent to a service worker, see synthesizeNotificationClick.

var client = null;
var currentTest = -1;

self.initialize = function() {
  return self.clients.matchAll().then(function(clients) {
    client = clients[0];
  });
};

self.postMessage = function(msg) {
  client.postMessage(msg);
};

// Run the next test in TESTS if any. Otherwise sends a 'quit' message. and
// stops.
// In order for that call to succeed, the script MUST have a TESTS array
// defined.
self.runNextTestOrQuit = function() {
  ++currentTest;
  if (currentTest >= TESTS.length) {
    client.postMessage('quit');
    return;
  }
  TESTS[currentTest]();
};

var pendingNotificationClickResolver = null;

// This method will use the |client| in order to synthesize a
// notificationclick event. The client will then use the testRunner.
// The returned promise will be resolved with the notificationclick
// event object.
self.synthesizeNotificationClick = function() {
  var promise = new Promise(function(resolve) {
    pendingNotificationClickResolver = resolve;
    var title = 'fake notification';
    registration.showNotification(title).then(function() {
      client.postMessage({type: 'click', title: title});
    });
  });

  return promise;
};

self.addEventListener('notificationclick', function(e) {
  if (!pendingNotificationClickResolver) {
    return;
  }
  var resolve = pendingNotificationClickResolver;
  pendingNotificationClickResolver = null;
  resolve(e);
  // To allow waitUntil to be called inside execution of the microtask
  // enqueued by above resolve function.
  e.waitUntil(Promise.resolve());
  e.notification.close();
});
