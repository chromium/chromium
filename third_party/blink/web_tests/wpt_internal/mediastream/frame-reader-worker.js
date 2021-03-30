var frameCount = 0;

self.addEventListener('message', function(e) {

  const frameStream = e.data.stream;
  const frameReader = frameStream.getReader();

  const framesToRead = 20;

  var closeStream = function() {
    frameReader.releaseLock();
    frameStream.cancel();
  }

  frameReader.read().then(function processFrame({done, value}) {
    if(done) {
      self.postMessage({ success: false, message: "Stream is ended before we could read enough frames" });
      closeStream();
      return;
    }

    if (value.codedWitdh == 0) {
      self.postMessage({ success: false, message: "Video frame is invalid" });
      closeStream();
      value.close();
      return;
    }

    value.close();

    if(++frameCount == framesToRead) {
      self.postMessage({ success: true, message: "Ran as expected" });
      closeStream();
      return;
    }

    frameReader.read().then(processFrame);
  })
}, false);
