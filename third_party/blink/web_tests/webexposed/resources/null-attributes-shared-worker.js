importScripts('../../resources/js-test.js');

// TODO(https://crbug.com/839117): Special logic is required to work around
// a bug where these interfaces are exposed in shared workers even though
// the Exposed attribute only specifies dedicated workers.

function runTest(platformSpecific) {
  shouldBe('navigator.serial', 'null');
  shouldBe('navigator.usb', 'null');
  shouldBe('navigator.wakeLock', 'null');
  finishJSTest();
}

self.onconnect = function(event) {
  self.postMessage = function(message) {
    event.ports[0].postMessage(message);
  };

  event.ports[0].onmessage = () => {
    runTest();
  };
};
