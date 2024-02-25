var frameCount = 0;

self.addEventListener('message', function(e) {

  const type = e.data.type;
  const frameStream = e.data.stream;
  const frameReader = frameStream.getReader();

  const framesToRead = 10;

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

    let isValid = true;
    if (type == "video") {
      if (value.codedWitdh == 0) {
        self.postMessage({ success: false, message: "Video frame is invalid" });
        isValid = false;
      }
    } else {
      if (!value.numberOfFrames) {
        self.postMessage({ success: false, message: "Audio data is invalid" });
        isValid = false;
      }
    }

    value.close();

    if (!isValid) {
      closeStream();
      return;
    }


    if(++frameCount == framesToRead) {
      self.postMessage({ success: true, message: "Ran as expected" });
      closeStream();
      return;
    }

    frameReader.read().then(processFrame);
  })
}, false);
