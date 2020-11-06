// Avoid polluting the global scope.
(function(globalObject) {

  // Save the list of property names of the global object before loading other scripts.
  var propertyNamesInGlobal = Object.getOwnPropertyNames(globalObject);

  importScripts('../../resources/js-test.js');
  importScripts('../../resources/global-interface-listing.js');

  function runTest(platformSpecific) {
    globalInterfaceListing(
        globalObject, propertyNamesInGlobal, platformSpecific, debug);
    finishJSTest();
  }

  if (self.postMessage) {
    self.onmessage = (e) => {
      runTest(e.data.platformSpecific);
    }
  } else {
    // Shared worker.  Make postMessage send to the newest client, which in
    // our tests is the only client.

    self.onconnect = function(event) {
      self.postMessage = function(message) {
        event.ports[0].postMessage(message);
      };

      event.ports[0].onmessage = (e) => {
        runTest(e.data.platformSpecific);
      };
    };
  }

})(this);
